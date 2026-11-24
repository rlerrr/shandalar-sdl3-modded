"""Exercise real loyalty costs and library-changing planeswalker abilities."""

import copy
import json
import re
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'loyalty_library_effects'
PROFILE = Path(__file__).parent / 'data/loyalty_library_effects_wave8_profiles.json'
# The full three-ability template controls enrollment and every secondary effect.
MODELS = {
    ('+1: Until your next turn, whenever a creature an opponent controls attacks, it gets -1/-0 until end of turn.',
     '−2: Reveal the top three cards of your library. An opponent separates those cards into two piles. '
     'Put one pile into your hand and the other on the bottom of your library in any order.',
     "−8: For each player, search that player's library for a nonland card and exile it, then that player shuffles. "
     'You may cast those cards without paying their mana costs.'):
        ('piles', 'jace_architect_of_thought'),
    ('+1: Look at the top two cards of your library. Put one of them into your graveyard.',
     "−3: Return another target nonland permanent to its owner's hand.",
     '−8: Each player shuffles their hand and graveyard into their library. You draw seven cards.'):
        ('wheel', 'jace_the_living_guildpact'),
    ('+1: Look at the top five cards of your library. You may reveal an artifact card from among them and put it into your hand. '
     'Put the rest on the bottom of your library in any order.',
     '−1: Target artifact becomes an artifact creature with base power and toughness 5/5.',
     '−4: Target player loses X life and you gain X life, where X is twice the number of artifacts you control.'):
        ('artifacts', 'tezzeret_agent_of_bolas'),
    ('+1: Create two 3/1 red Elemental creature tokens with haste. Exile them at the beginning of the next end step.',
     '0: Discard all the cards in your hand, then draw that many cards plus one.',
     '−X: Chandra deals X damage to each creature.'):
        ('elementals', 'chandra_flamecaller'),
}
COMMON = ('short', 'countered', 'replay', 'once_per_turn', 'opposing_turn')
SCENARIOS = {
    'piles': ('pick_pile', 'empty_library', 'one_card', 'weaken', 'weaken_own', 'search_decline', 'search_cast', 'no_nonland'),
    'wheel': ('top_two', 'one_card', 'empty_library', 'bounce_own', 'bounce_foe', 'wheel'),
    'artifacts': ('look_five', 'decline_artifact', 'no_artifact', 'animate_own', 'animate_foe', 'drain', 'no_artifacts'),
    'elementals': ('tokens', 'tokens_combat', 'wheel', 'empty_hand', 'sweep_one', 'sweep_two', 'sweep_all', 'too_much_loyalty'),
}
WITNESSES = frozenset(('Island', 'Forest', 'Mountain', 'Swamp', 'Counterspell', 'Boomerang', 'Goblin Roughrider',
                      'Mogg Fanatic', 'Goblin Piker', 'Wind Drake', 'Hill Giant', 'Darksteel Ingot', 'Sol Ring',
                      'Ornithopter', 'Elemental'))
DUMMIES = frozenset(('Goblin Roughrider', 'Goblin Piker', 'Wind Drake', 'Hill Giant', 'Ornithopter'))


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = tuple(record['oracle_text'].splitlines())
    printed_type = record['type_line'].partition(' — ')[0].split()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or record['keywords']
            or printed_type not in (['Legendary', 'Planeswalker'], ['Planeswalker']) or lines not in MODELS
            or not re.fullmatch('[1-9][0-9]*', record.get('loyalty', ''))):
        raise ValueError('complete normal loyalty/library effect required')
    costs = []
    for line in lines:
        fee = line.partition(':')[0].replace('−', '-')
        costs.append(fee if fee == '-X' else int(fee))
    return dict(mode=MODELS[lines][0], mana=cost_from_oracle(record), start=int(record['loyalty']), costs=costs, keywords=[])


def handler(record):
    return 'manalinkeh_card_' + MODELS[tuple(record['oracle_text'].splitlines())][1]


def variants(params):
    return [f'{scene}_{seat}' for scene in (*COMMON, *SCENARIOS[params['mode']]) for seat in (0, 1)]


def spec(declaration, record):
    params = classify(record)
    source = profiles()['sources'].get(record['name'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not source or source['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('exact pinned planeswalker and loyalty costs required')
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct approved loyalty variants required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    p = profiles()
    if p.get('version') != 1 or set(p['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned loyalty witnesses required')
    for group in ('sources', 'witnesses'):
        for name, pin in p[group].items():
            row = registry.get(name, {})
            token = group == 'witnesses' and name == 'Elemental'
            allowed_type = row.get('kind') in ('card', 'token') if token else row.get('kind') == 'card'
            native = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or not allowed_type
                    or token and (row['csvid'] != 18726 or pin['oracle'].get('layout') != 'token')
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not native):
                raise ValueError(f'changed planeswalker or witness binding: {name}')
            if group == 'sources':
                classify(oracle[name])
                if row['handler'] != handler(oracle[name]):
                    raise ValueError(f'changed planeswalker source callback: {name}')


class _World(_SpellWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.loyalty = 0
        self.shuffles = [0, 0]
        self.shuffled = set()

    def active(self):
        return self.cards['source'][2] == 'battlefield'

    def observe_loyalty(self):
        self.emit('assert_counter', ref='source', counter_kind='loyalty', value=self.loyalty)

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        if self.active():
            self.observe_loyalty()
        names = sorted({card['name'] for card in self.setup} | {
            n for planes in self.zones.values() for zone in planes for n in zone})
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            self.emit('assert_shuffle_count', player=seat, value=self.shuffles[seat])
            for color, mana in enumerate(self.mana[seat]):
                self.emit('assert_mana', player=seat, color=color, value=mana)
            for zone, players in self.zones.items():
                cards = players[seat]
                self.emit('assert_zone', player=seat, zone=zone, value=len(cards))
                if zone == 'library' and seat not in self.shuffled:
                    for index, card in enumerate(cards):
                        self.emit('assert_zone_card', player=seat, zone=zone, index=index, value=self.rows[card]['csvid'])
                else:
                    for card in names:
                        self.emit('assert_zone_multiplicity', player=seat, zone=zone, index=self.rows[card]['csvid'], value=cards.count(card))
        for ref, creature in self.entities.items():
            if creature['zone'] == 'battlefield':
                for prop in ('power','toughness','tapped','damage'):
                    self.observe(ref, prop, creature[prop])

    def enter(self, scene):
        budget = list(self.p['mana'])
        if scene == 'short':
            budget[0] -= 1
        self.cast('source', budget=budget, denied=scene == 'short')
        if scene == 'countered':
            self.emit('announce_spell', ref='source')
            self.cast('reply', parent='source')
            self.finish('reply', 'graveyard', response=True)
            self.move('source', 'graveyard')
        elif scene != 'short':
            self.finish('source', 'battlefield')
            self.loyalty = self.p['start']
        self.receipts()

    def next_main(self):
        if any(map(any, self.mana)):
            raise ValueError('actual turn bridge needs spent mana')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        self.emit('turn', player=self.current, value=0)
        for creature in self.entities.values():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature['tapped'] = False

    def activate(self, mode, *, target=None, player=None, amount=None, accepted=True, resolution=()):
        self.emit('can_activate', ref='source', value=1)
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.owner, value=mode)
        if target:
            self.emit('choose_target', ref=target, player=self.owner)
        if player is not None:
            self.emit('choose_player', player=self.owner, value=player)
        fee = self.p['costs'][mode]
        if fee == '-X':
            self.emit('choose_number', player=self.owner, value=amount)
            fee = -amount
        self.emit('activate', ref='source', value=int(accepted))
        if accepted:
            self.loyalty += fee
        if self.loyalty:
            self.observe_loyalty()
        self.emit('assert_stack', value=int(accepted))
        if accepted:
            self.steps.extend(resolution)
            self.emit('resolve')
            if not self.loyalty:
                self.move('source', 'graveyard')
            else:
                self.emit('can_activate', ref='source', value=0)
        self.emit('present_ai_dialogs', value=0)
        self.emit('assert_unused_choices', value=0)

    def charge(self, fee):
        while self.loyalty < fee:
            mode, choices = self.prepare_plus()
            self.activate(mode, resolution=choices)
            self.next_main()
            self.next_main()

    def prepare_plus(self):
        from recipes import step

        mode = self.p['mode']
        deck = self.zones['library'][self.owner]
        if mode == 'wheel' and deck:
            moved = deck.pop(0)
            self.zones['graveyard'][self.owner].append(moved)
            return 0, [step('choose_zone_card', player=self.owner, value=self.rows[moved]['csvid'])]
        if mode == 'artifacts' and deck:
            return 0, self.prepare_artifact(False)
        return 0, []

    def prepare_artifact(self, accept):
        from recipes import step

        deck = self.zones['library'][self.owner]
        prefix = deck[:5]
        if not prefix:
            return []
        target = 'Sol Ring'
        if accept:
            if target not in prefix:
                raise ValueError('artifact must be in the observed prefix')
            prefix.remove(target)
            self.zones['hand'][self.owner].append(target)
        choice = step('choose_zone_card', player=self.owner, value=self.rows[target]['csvid']) if accept else step('choose_zone', player=self.owner, value=-1)
        self.zones['library'][self.owner] = deck[5:] + prefix
        return [choice, *[step('choose_zone', player=self.owner, value=0) for _ in prefix[1:]]]

    def attack_player(self, refs, amount, power=()):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref=ref, value=1)
        self.emit('finish_attackers')
        for ref, value in power:
            self.observe(ref, 'power', value)
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[1-self.current] -= amount
        for ref in refs:
            if ref in self.entities:
                self.entities[ref]['tapped'] = True

    def draw(self, amount):
        for _ in range(amount):
            self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))

    def setup_cards(self, items, seat, zone='library'):
        for index, card in enumerate(items):
            self.add(card, f'{zone}_{seat}_{index}', seat, zone)


def fixture(w, scene):
    w.add(w.name, 'source', w.owner, 'hand')
    if scene == 'countered':
        w.add('Counterspell', 'reply', w.foe, 'hand')
    mode, owner, foe = w.p['mode'], w.owner, w.foe
    if mode == 'piles':
        if scene in ('pick_pile','replay','once_per_turn'):
            w.setup_cards(['Island'] * 3 + ['Mountain'], owner)
        elif scene == 'one_card':
            w.setup_cards(['Island'], owner)
        elif scene in ('search_decline','search_cast','no_nonland'):
            for seat, creature in enumerate(('Goblin Piker','Wind Drake')):
                w.setup_cards(['Island', creature] if scene != 'no_nonland' else ['Island'], seat)
        if scene == 'weaken':
            w.add('Goblin Roughrider', 'attacker', foe)
            w.add('Wind Drake', 'nonattacker', foe)
            w.add('Boomerang', 'bounce', foe, 'hand')
        elif scene == 'weaken_own':
            w.add('Goblin Roughrider', 'attacker', owner)
    elif mode == 'wheel':
        if scene in ('top_two','replay','once_per_turn'):
            w.setup_cards(['Island','Forest','Mountain'], owner)
        elif scene == 'one_card':
            w.setup_cards(['Island'], owner)
        elif scene == 'wheel':
            # Exactly seven cards after zero-loyalty Jace joins the shuffle.
            for seat, count in ((owner, 3), (foe, 2)):
                w.setup_cards(['Island'] * count, seat)
                w.setup_cards(['Mountain'], seat, 'hand')
                w.setup_cards(['Forest','Swamp'], seat, 'graveyard')
        if scene in ('bounce_own','bounce_foe'):
            w.add('Darksteel Ingot', 'target', owner if scene == 'bounce_own' else foe)
            w.add('Island', 'land', foe)
    elif mode == 'artifacts':
        if scene in ('look_five','decline_artifact','no_artifact','replay','once_per_turn'):
            w.setup_cards(['Island', 'Sol Ring' if scene != 'no_artifact' else 'Mountain',
                           'Island', 'Island', 'Island', 'Forest'], owner)
        if scene in ('animate_own','animate_foe'):
            w.add('Darksteel Ingot', 'artifact', owner if scene == 'animate_own' else foe)
            if scene == 'animate_foe':
                w.add('Boomerang', 'bounce', foe, 'hand')
        if scene in ('drain','no_artifacts'):
            w.add('Ornithopter', 'other_artifact', foe)
            if scene == 'drain':
                w.add('Darksteel Ingot', 'artifact', owner)
                w.add('Sol Ring', 'own_artifact', owner)
    elif mode == 'elementals':
        if scene in ('wheel','empty_hand','replay','once_per_turn'):
            w.setup_cards(['Island','Forest','Mountain','Swamp'], owner)
            if scene != 'empty_hand':
                w.setup_cards(['Goblin Piker','Sol Ring'], owner, 'hand')
            w.setup_cards(['Forest'], foe, 'hand')
        if scene.startswith('sweep') or scene == 'too_much_loyalty':
            w.add('Mogg Fanatic','small',owner)
            w.add('Goblin Roughrider','medium',foe)
            w.add('Hill Giant','large',owner)
            w.add('Darksteel Ingot','artifact',foe)


def piles(w, scene):
    from recipes import step

    owner = w.owner
    lib = w.zones['library'][owner]
    if scene.startswith('weaken'):
        w.activate(0)
        if scene == 'weaken_own':
            w.attack_player(['attacker'], 3, power=(('attacker',3),))
            return
        w.next_main()
        w.cast('bounce', target='source')
        w.finish('bounce', 'graveyard')
        w.move('source', 'hand')
        w.attack_player(['attacker'], 2, power=(('attacker',2),('nonattacker',2)))
        w.next_main()
        w.next_main()
        w.attack_player(['attacker'], 3, power=(('attacker',3),))
        return
    if scene in ('search_decline','search_cast','no_nonland'):
        w.charge(8)
        choices = [step('observe_search_filters', value=1)]
        for seat, creature in enumerate(('Goblin Piker','Wind Drake')):
            if scene == 'no_nonland':
                choices.append(step('choose_zone', player=owner, value=-1))
            else:
                choices.append(step('choose_zone_card', player=owner, value=w.rows[creature]['csvid']))
                lib_for = w.zones['library'][seat]
                lib_for.remove(creature)
                w.zones['exile'][seat].append(creature)
            w.shuffles[seat] += 1
        if scene == 'search_decline':
            choices.append(step('choose_dialog', player=owner, value=2))
        elif scene == 'search_cast':
            choices.extend([step('choose_dialog', player=owner, value=0), step('choose_dialog', player=owner, value=0)])
        w.activate(2, resolution=choices)
        w.emit('assert_offered_legal_card', index=w.rows['Island']['csvid'], value=0)
        if scene != 'no_nonland':
            w.emit('assert_offered_legal_card', index=w.rows['Wind Drake']['csvid'], value=1)
        w.emit('observe_search_filters', value=0)
        if scene == 'search_cast':
            for seat, creature in enumerate(('Goblin Piker','Wind Drake')):
                w.zones['exile'][seat].remove(creature)
                w.zones['battlefield'][owner].append(creature)
                w.emit('bind_permanent_copy', ref=f'free_{seat}', index=0, player=owner, value=w.rows[creature]['csvid'])
                w.observe(f'free_{seat}', 'in_play', 1)
        return
    if not lib:
        w.activate(1)
        return
    choices = []
    if len(lib) == 1:
        choices.append(step('choose_zone', player=0, value=-1))
        take = 1
    elif owner == 0:
        # Equal copies split 2/1; a click changes displayed pile, Done accepts it.
        choices.extend([step('choose_zone', player=0, value=0), step('choose_zone', player=0, value=-1),
                        step('choose_zone', player=0, value=1)])
        take = 1
    else:
        # Human opponent moves exactly one card to pile 1; acknowledge AI choice.
        choices.extend([step('choose_zone', player=0, value=2), step('choose_zone', player=0, value=-1),
                        step('choose_zone', player=0, value=-1)])
        take = 2
    w.activate(1, resolution=choices)
    count = min(3, len(lib))
    revealed = lib[:count]
    lib[:count] = []
    w.zones['hand'][owner].extend(revealed[:take])
    lib.extend(revealed[take:])


def guildpact(w, scene):
    from recipes import step

    if scene.startswith('bounce'):
        w.activate(1, target='target')
        w.move('target','hand')
    elif scene == 'wheel':
        w.charge(8)
        w.activate(2)
        for seat in (0,1):
            deck = w.zones['library'][seat]
            for zone in ('hand','graveyard'):
                deck.extend(w.zones[zone][seat])
                w.zones[zone][seat].clear()
            w.shuffles[seat] += 1
            w.shuffled.add(seat)
        w.draw(7)
    else:
        deck = w.zones['library'][w.owner]
        if not deck:
            w.activate(0)
        else:
            moved = deck.pop(1 if len(deck) > 1 else 0)
            w.activate(0, resolution=[step('choose_zone_card', player=w.owner,value=w.rows[moved]['csvid'])])
            w.zones['graveyard'][w.owner].append(moved)


def tezzeret(w, scene):
    if scene.startswith('animate'):
        w.activate(1, target='artifact')
        w.observe('artifact', 'power', 5)
        w.observe('artifact', 'toughness', 5)
        seat = w.cards['artifact'][1]
        w.emit('assert_matching_permanents', player=seat, index=w.rows['Darksteel Ingot']['csvid'], property='artifact', value=1)
        w.next_main()
        if seat == w.owner:
            w.next_main()
        else:
            w.cast('bounce', target='source')
            w.finish('bounce', 'graveyard')
            w.move('source', 'hand')
        w.attack_player(['artifact'], 5, power=(('artifact',5),))
        w.observe('artifact', 'tapped', 1)
    elif scene in ('drain','no_artifacts'):
        w.charge(4)
        w.activate(2, player=w.foe)
        amount = 4 if scene == 'drain' else 0
        w.life[w.owner] += amount
        w.life[w.foe] -= amount
    else:
        accepts = scene not in ('decline_artifact','no_artifact') and bool(w.zones['library'][w.owner])
        w.activate(0, resolution=w.prepare_artifact(accepts))


def chandra(w, scene):
    from recipes import step

    if scene.startswith('tokens'):
        w.activate(0)
        w.zones['battlefield'][w.owner].extend(['Elemental'] * 2)
        token = w.rows['Elemental']['csvid']
        refs = [f'token_{i}' for i in (0,1)]
        for i, ref in enumerate(refs):
            w.emit('bind_permanent_copy', player=w.owner, ref=ref, index=i, value=token)
            w.observe(ref,'power',3)
            w.observe(ref,'toughness',1)
        for prop, value in (('token',1),('color',16),('subtype_Elemental',1)):
            w.emit('assert_matching_permanents', player=w.owner, index=token, property=prop, value=value)
        w.receipts()
        if scene == 'tokens_combat':
            w.attack_player(refs,6)
            w.receipts()
        w.next_main()
        w.zones['battlefield'][w.owner] = [n for n in w.zones['battlefield'][w.owner] if n != 'Elemental']
        for ref in refs:
            w.observe(ref, 'in_play', 0)
        w.emit('assert_zone_multiplicity', player=w.owner, zone='battlefield', index=token, value=0)
    elif scene.startswith('sweep') or scene == 'too_much_loyalty':
        amount = dict(sweep_one=1,sweep_two=2,sweep_all=4,too_much_loyalty=5)[scene]
        accepted = amount <= w.loyalty
        w.activate(2, amount=amount, accepted=accepted)
        if accepted:
            for ref, creature in w.entities.items():
                creature['damage'] += amount
                if creature['damage'] >= creature['toughness']:
                    w.move(ref, 'graveyard')
    else:
        hand = w.zones['hand'][w.owner]
        # The human discard-all path asks for each card; the AI chooses internally.
        choices = [step('choose_zone_card', player=w.owner, value=w.rows[name]['csvid'])
                   for name in hand] if w.owner == 0 else []
        w.activate(1, resolution=choices)
        size = len(hand)
        w.zones['graveyard'][w.owner].extend(hand)
        hand.clear()
        w.draw(size + 1)


def build_variant(name, params, scene, owner, registry, oracle):
    from recipes import step

    w = _World(name,params,owner,registry,oracle)
    fixture(w,scene)
    w.emit('turn', player=owner, value=0)
    for seat in (0,1):
        w.pool(seat,[0]*6)
    w.enter(scene)
    if scene in ('short','countered'):
        return w.setup,w.steps
    if scene == 'opposing_turn':
        w.next_main()
        w.emit('can_activate',ref='source',value=0)
        w.emit('activate',ref='source',value=0)
        w.receipts()
        return w.setup,w.steps
    if scene == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    abilities = {'piles':piles, 'wheel':guildpact, 'artifacts':tezzeret, 'elementals':chandra}
    abilities[params['mode']](w,scene)
    w.receipts()
    if scene == 'once_per_turn':
        w.emit('activate', ref='source', value=0)
        w.next_main()
        w.emit('can_activate',ref='source',value=0)
        w.next_main()
        w.emit('can_activate',ref='source',value=1)
        w.receipts()
    if scene == 'replay':
        repeat = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'),step('ai_roundtrip'),step('restore'),*repeat,step('assert_recorded_state')])
    return w.setup,w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    record,row = oracle[name],registry[name]
    pin = profiles()['sources'].get(name)
    if not pin or row != pin['registry'] or row['csvid'] != csvid or row['handler'] != handler(record):
        raise ValueError(f'unpinned planeswalker source: {name}')
    cases = []
    for variant in selected:
        scene, owner = variant.rsplit('_',1)
        setup,steps = build_variant(name,params,scene,int(owner),registry,oracle)
        cases.append(dict(name=f'LoyaltyLibraryEffectsRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
            setup=setup, steps=steps, _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                oracle_text=declaration['oracle_text'], untested_components=['multiplayer','GUI presentation','AI decision strategy',
                    'legacy spell-type eligibility across other Jace ultimate witnesses'])))
    return cases
