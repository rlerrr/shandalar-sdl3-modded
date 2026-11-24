"""Whole-card clash effects using the actual reveal and bottom selectors."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'clash_whole_cards'
PROFILE_PATH = Path(__file__).with_name('data') / 'clash_whole_cards_wave8_profiles.json'
REMINDER = (' (Each clashing player reveals the top card of their library, then puts that card on their choice of the top or bottom. '
            'A player wins if their card had a greater mana value.')
CLASH = 'Clash with an opponent.'
RETURN = "If you win, return {name} to its owner's hand."
CLAUSES = {
    'counter': 'When this creature enters, clash with an opponent. If you win, put a +1/+1 counter on this creature.' + REMINDER + ')',
    'ambush': 'Create two 1/1 green Elf Warrior creature tokens. ' + CLASH + ' If you win, those creatures gain deathtouch until end of turn.' +
              REMINDER + ' Any amount of damage a creature with deathtouch deals to a creature is enough to destroy it.)',
    'greed': 'You lose 2 life and draw two cards, then clash with an opponent. If you win, repeat this process.' + REMINDER + ')',
    'lash': '{name} deals 3 damage to target creature. ' + CLASH + " If you win, {name} deals 3 damage to that creature's controller." + REMINDER + ')',
    'protect': 'Target creature you control gains protection from the color of your choice until end of turn. ' + CLASH + ' ' + RETURN + REMINDER + ')',
    'ants': '{name} deals 1 damage to any target. ' + CLASH + ' ' + RETURN + REMINDER + ')',
    'draw': 'Draw a card. ' + CLASH + ' ' + RETURN + REMINDER + ')',
    'revive': "Return target creature card from a graveyard to its owner's hand. " + CLASH + ' ' + RETURN + REMINDER + ')',
}
RETURNS = ('protect', 'ants', 'draw', 'revive')
TARGETS = ('lash', 'protect', 'ants', 'revive')
KINDS = dict(counter='Creature', ambush='Kindred Instant — Elf', greed='Sorcery', lash='Instant', protect='Instant',
             ants='Instant', draw='Sorcery', revive='Sorcery')
EXTRA = {'counter': (), 'ambush': ('expiry',), 'greed': (),
         'lash': ('own_target', 'lethal', 'fizzle', 'wrong_type'), 'protect': ('expiry', 'fizzle', 'wrong_type'),
         'ants': ('creature', 'walker', 'fizzle'), 'draw': (), 'revive': ('own_grave', 'fizzle', 'wrong_type')}
MODES = ('win', 'tie', 'loss', 'human_bottom', 'snapshot')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    name = record.get('name', '')
    effect = next((key for key, clause in CLAUSES.items() if record.get('oracle_text') == clause.format(name=name)), None)
    mana = record.get('mana_cost', '')
    line = record.get('type_line', '')
    expected_kind = KINDS.get(effect)
    correct_type = line.startswith('Creature — ') if effect == 'counter' else line == expected_kind
    if (not effect or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not correct_type or not re.fullmatch(r'\{[1-9]\}\{[WUBRG]\}', mana)
            or record.get('colors') != [mana[-2]] or record.get('keywords') != ['Clash']):
        raise ValueError('complete ordinary printed clash spell and native cost required')
    return dict(effect=effect, mana=mana, type_line=line, power=int(record['power']) if effect == 'counter' else None,
                toughness=int(record['toughness']) if effect == 'counter' else None)


def variants(params):
    effect = params['effect']
    variants_ = MODES + EXTRA[effect] + (('recast',) if effect in RETURNS else ())
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in variants_]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or declaration.get('params') != params):
        raise ValueError('complete pinned Clash text and typed parameters required')
    choices = declaration.get('variants')
    if not isinstance(choices, list) or not choices or any(type(choice) is not str for choice in choices):
        raise ValueError('explicit clash variants required')
    if len(set(choices)) != len(choices) or not set(choices) <= set(variants(params)):
        raise ValueError('unique reviewed clash variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unused = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    pending = {entry['csvid'] for entry in unused.values()}
    static = set(data['static_characteristics_only_witnesses'])
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('clash source or witness changed: ' + name)
        if name not in static and (row['csvid'] in pending or 'dummy' in row['handler']):
            raise ValueError('clash action witness must be implemented: ' + name)
    token = data['token']
    if rows.get('Elf Warrior') != token['registry'] or oracle.get('Elf Warrior') != token['oracle']:
        raise ValueError('pinned Elf Warrior token identity required')
    root = Path(__file__).resolve().parents[4]
    for name, pin in data['sources'].items():
        source = pin['compiled_source']
        if hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('clash compiled source changed: ' + name)


class ClashBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.d, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.effect = params['effect']
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life, self.mana, self.pools = [20, 20], [0, 0], [[0] * 6, [0] * 6]
        self.zones = []
        self.target_owners = []
        self.loyalty = None
        self.setup_zones()
        self.add(name, 'source', 0, 'hand')
        self.witnesses()

    def setup_zones(self):
        if self.mode == 'loss':
            tops = ['Forest', 'Hill Giant']
        elif self.mode == 'tie':
            tops = ['Forest', 'Island']
        else:
            tops = ['Craw Wurm', 'Island']
        for player, top in enumerate(tops):
            prefix = []
            if not player and self.effect in ('draw', 'greed'):
                prefix = ['Plains'] if self.effect == 'draw' else ['Plains', 'Mountain']
            tail = ['Hill Giant', 'Forest', 'Plains', 'Mountain', 'Island'] if self.effect == 'draw' else ['Forest', 'Plains', 'Mountain', 'Island']
            zones = dict(hand=['Forest', 'Plains'], library=prefix + [top] + tail, graveyard=['Island'], exile=['Mountain'])
            self.zones.append(zones)
            for zone, cards in zones.items():
                self.setup.extend(self.card(name, f'stock_{zone}_{player}_{index}', player, zone) for index, name in enumerate(cards))
        # Four tapped Islands plus two stocked lands in hand expose the AI's
        # actual optional placement picker. No land is spent or drawn here.
        for index in range(4):
            self.add('Island', f'quiet_land_{index}', 1-self.owner, state=['tapped'])

    def grave_card(self, name, player, index):
        self.setup.append(self.card(name, f'grave_target_{index}', player, 'graveyard'))
        self.zones[player]['graveyard'].append(name)

    def witnesses(self):
        mode, effect = self.mode, self.effect
        if effect == 'revive' and mode != 'wrong_type':
            self.target_owners = [0] if mode == 'own_grave' else [1]
            if mode == 'recast':
                self.target_owners.append(0)
            for index, player in enumerate(self.target_owners):
                self.grave_card('Hill Giant' if player == 0 else 'Grizzly Bears', player, index)
        if effect == 'ambush':
            for index in range(2):
                self.add('Crater Elemental', f'other_{index}', 1)
        if effect == 'protect' and mode != 'wrong_type':
            self.add('Crater Elemental', 'target_0')
            if mode == 'recast':
                self.add('Grizzly Bears', 'target_1')
            self.add('Lightning Bolt', 'red_probe', 1, 'hand')
            self.add('Unsummon', 'blue_probe', 1, 'hand')
        if effect == 'lash' and mode != 'wrong_type':
            self.add('Grizzly Bears' if mode == 'lethal' else 'Crater Elemental', 'target_0', int(mode != 'own_target'))
        if effect == 'ants' and mode in ('creature', 'fizzle', 'walker'):
            self.add('Jace Beleren' if mode == 'walker' else 'Crater Elemental', 'target_0', 1)
            if mode == 'walker':
                self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        if mode == 'wrong_type' and effect in ('protect', 'lash'):
            self.add('Crater Elemental' if effect == 'protect' else 'Howling Mine', 'excluded', 1)
        if mode == 'fizzle':
            self.add('Reclaim' if effect == 'revive' else 'Unsummon', 'reply', 1, 'hand')

    def pools_receipt(self):
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)

    def queue_round(self, libraries, bottom):
        start = len(self.steps)
        if any(not deck for deck in libraries):
            raise ValueError('reviewed clash round requires a real reveal from both libraries')
        top = [deck[0] for deck in libraries]
        self.emit('choose_dialog', player=0, value=0)
        self.emit('choose_dialog', player=0, value=0)
        human = self.owner
        # clash asks the source's controller first. The human can move their
        # revealed card. The AI path is an optional tutor at six or more
        # lands in hand and in play combined, so Done lawfully keeps it.
        for player in (0, 1):
            if player == human and bottom:
                self.emit('choose_zone_card', player=player, value=self.rows[top[player]]['csvid'])
                libraries[player].append(libraries[player].pop(0))
            else:
                self.emit('choose_zone', player=player, value=-1)
        won = float(self.oracle[top[0]]['cmc']) > float(self.oracle[top[1]]['cmc'])
        return won, len(self.steps) - start

    def queue_target(self, index):
        effect = self.effect
        if effect in ('lash', 'protect') or effect == 'ants' and 'target_0' in self.entities:
            self.emit('choose_target', player=0, ref=f'target_{index}')
        elif effect == 'ants':
            self.emit('choose_player', player=0, value=1-index)
        elif effect == 'revive':
            player = self.target_owners[index]
            if len(self.target_owners) == 2 and index == 0:
                self.emit('choose_player', player=0, value=player)
            self.emit('choose_zone_card', player=0, value=self.rows['Hill Giant' if not player else 'Grizzly Bears']['csvid'])

    def cast_source(self, index):
        self.queue_target(index)
        self.emit('cast', ref='source', value=1)
        self.spend(self.p['mana'])
        self.emit('assert_stack', value=1)
        self.emit('assert_card', ref='source', property='in_hand', value=0)
        self.emit('assert_unused_choices', value=0)
        self.pools_receipt()

    def place_tokens(self):
        for index in range(2):
            ref = f'elf_{index}'
            self.tokens.append(ref)
            # The inherited token receipt hardcodes Zombie; use real Elf
            # identity and subtype receipts in this family.
            self.entities[ref] = dict(name='Elf Warrior', owner=0, player=0, zone='battlefield', creature=True,
                                      power=1, toughness=1, boost=0, counters=0, damage=0, tapped=False, token=False)

    def simulate_clash(self):
        effect = self.effect
        libraries = [zone['library'] for zone in self.zones]
        if effect == 'protect':
            self.emit('choose_color', player=0, value=4)
        if effect == 'draw':
            self.draw(0, 1)
        if effect == 'greed':
            self.draw(0, 2)
            self.life[0] -= 2
        won, _ = self.queue_round(libraries, self.mode == 'human_bottom')
        if effect == 'greed' and won:
            self.draw(0, 2)
            self.life[0] -= 2
            won_again, _ = self.queue_round(libraries, False)
            if won_again:
                raise ValueError('mandatory repeat fixture must terminate through a real second comparison')
        return won

    def normal_effect(self, index, won):
        effect = self.effect
        target = f'target_{index}'
        if effect == 'counter':
            self.move('source', 'battlefield')
            self.entities['source']['counters'] += int(won)
            return
        if effect == 'ambush':
            self.place_tokens()
        if effect == 'lash':
            entity = self.entities[target]
            entity['damage'] += 3
            self.life[entity['player']] -= 3 * int(won)
            if entity['damage'] >= entity['toughness']:
                self.move(target, 'graveyard')
        if effect == 'ants':
            if self.mode == 'walker':
                self.loyalty -= 1
            elif target in self.entities:
                self.entities[target]['damage'] += 1
            else:
                self.life[1-index] -= 1
        if effect == 'revive':
            player = self.target_owners[index]
            target_name = 'Grizzly Bears' if player else 'Hill Giant'
            self.zones[player]['graveyard'].remove(target_name)
            self.zones[player]['hand'].append(target_name)
            self.emit('bind_hand', ref=f'recovered_{index}', player=player, value=self.rows[target_name]['csvid'])
            self.emit('assert_card', ref=f'recovered_{index}', property='in_hand', value=1)
        if effect in RETURNS and won:
            self.emit('bind_hand', ref='source', player=0, value=self.csvid)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        else:
            self.move('source', 'graveyard')

    def fizzle(self):
        self.emit('announce_spell', ref='source')
        if self.effect == 'revive':
            reply = 'Reclaim'
            self.emit('choose_zone_card', player=1, value=self.rows['Grizzly Bears']['csvid'])
        else:
            reply = 'Unsummon'
            self.emit('choose_target', player=1, ref='target_0')
        self.emit('cast_response', ref='reply', value=1)
        self.spend(self.oracle[reply]['mana_cost'], 1)
        self.emit('assert_stack', value=2)
        hypothetical = [zone['library'][:] for zone in self.zones]
        if self.effect == 'revive':
            self.zones[1]['graveyard'].remove('Grizzly Bears')
            self.zones[1]['library'].insert(0, 'Grizzly Bears')
            hypothetical[1].insert(0, 'Grizzly Bears')
        else:
            self.move('target_0', 'hand')
        _, queued = self.queue_round(hypothetical, False)
        self.emit('resolve_response')
        # Correct all-targets-illegal resolution cannot consume a clash plan.
        # If the legacy callback nevertheless presents it, the actual legal
        # acknowledgments prevent the rules defect from becoming a hard error.
        self.emit('assert_unused_choices', value=queued)
        self.move('reply', 'graveyard')
        self.move('source', 'graveyard')

    def protection_checks(self, index):
        target = f'target_{index}'
        self.emit('can_target_creature', ref='red_probe', other=target, value=0)
        self.emit('can_target_creature', ref='blue_probe', other=target, value=1)
        if self.mode == 'expiry':
            self.emit('cleanup')
            self.emit('refresh')
            self.emit('can_target_creature', ref='red_probe', other=target, value=1)

    def token_combat(self, lethal):
        for prop, value in (('token', 1), ('color', 8), ('subtype_Elf', 1), ('subtype_Warrior', 1)):
            self.emit('assert_matching_permanents', player=0, index=self.rows['Elf Warrior']['csvid'], property=prop, value=value)
        self.bind_tokens()
        if self.mode == 'expiry':
            self.emit('advance_turn')
            self.emit('advance_turn')
            lethal = False
        self.emit('begin_attackers')
        for index in range(2):
            ref = f'other_{index}'
            self.emit('attack', ref=ref, value=1)
            self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        for index in range(2):
            self.emit('block', ref=f'elf_{index}', other=f'other_{index}', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for index in range(2):
            ref = f'other_{index}'
            if lethal:
                self.move(ref, 'graveyard')
            else:
                self.entities[ref]['damage'] += 1

    def resolve_source(self, index):
        if self.mode == 'fizzle':
            self.fizzle()
            return
        won = self.simulate_clash()
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.normal_effect(index, won)
        if self.effect == 'protect':
            self.protection_checks(index)
        if self.loyalty is not None:
            self.emit('assert_counter', ref='target_0', counter_kind='loyalty', value=self.loyalty)
        if self.effect == 'ambush':
            self.token_combat(won)

    def run(self):
        self.emit('turn', player=int(self.effect == 'ambush'), value=0)
        self.emit('present_ai_dialogs', value=1)
        recast = self.mode == 'recast'
        count = 2 if recast else 1
        self.set_pool(0, [count*value for value in self.cost(self.oracle[self.name])])
        reply = self.entities.get('reply')
        self.set_pool(1, self.cost(self.oracle[reply['name']]) if reply else [0]*6)
        self.emit('refresh')
        cut = None
        if self.mode == 'wrong_type':
            self.emit('can_cast', ref='source', value=0)
        else:
            self.cast_source(0)
            if self.mode == 'snapshot':
                cut = len(self.steps)
            self.resolve_source(0)
            if recast:
                self.receipts()
                self.cast_source(1)
                self.resolve_source(1)
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'ClashWholeCardsRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text'],
                    untested_components=['Empty library comparisons need deterministic native semantics.']))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('unique approved clash variants required')
    if rows[name]['csvid'] != csvid:
        raise ValueError('clash source identity changed')
    return [ClashBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
