"""Reveal-land entry, exact hand filters and actual chosen mana output."""

import copy
import json
import re
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'hand_reveal_dual_land'
COLORS = 'CBUGRW'
LAND_TYPES = {'Plains', 'Island', 'Swamp', 'Mountain', 'Forest'}
CREATURE_TYPES = {'Giant', 'Goblin', 'Faerie', 'Merfolk'}
ENTRY = re.compile(r"As this land enters, you may reveal a ([A-Za-z]+)(?: or ([A-Za-z]+))? card from your hand\. If you don't, this land enters tapped\.")
MANA = re.compile(r'\{T\}: Add \{([WUBRG])\} or \{([WUBRG])\}\.')
SPEND = {'B': 'Typhoid Rats', 'U': 'Merfolk of the Pearl Trident', 'G': 'Jungle Lion',
         'R': 'Raging Goblin', 'W': 'Savannah Lions'}


def profiles():
    path = Path(__file__).parent / 'data/hand_reveal_dual_land_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def classify(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] != 'Land' or record.get('mana_cost') or record.get('keywords')):
        raise ValueError('whole normal standalone land required')
    lines = record['oracle_text'].splitlines()
    if len(lines) != 2 or not (entry := ENTRY.fullmatch(lines[0])) or not (mana := MANA.fullmatch(lines[1])):
        raise ValueError('complete optional reveal and dual-color tap wording required')
    subtypes = [item for item in entry.groups() if item]
    outputs = list(mana.groups())
    if len(set(subtypes)) != len(subtypes) or len(set(outputs)) != 2:
        raise ValueError('distinct printed reveal types and mana colors required')
    if len(subtypes) == 2 and set(subtypes) <= LAND_TYPES:
        kind = 'land_types'
    elif len(subtypes) == 1 and subtypes[0] in CREATURE_TYPES:
        kind = 'creature_type'
    else:
        raise ValueError('reviewed subtype classes only')
    return dict(reveal_kind=kind, subtypes=subtypes, outputs=outputs)


def matches(record, params):
    category, _, printed = record['type_line'].partition(' — ')
    if params['reveal_kind'] == 'land_types':
        return 'Land' in category.split() and bool(set(printed.split()) & set(params['subtypes']))
    return any(kind in category.split() for kind in ('Creature', 'Kindred')) and (
        'Changeling' in record['keywords'] or bool(set(printed.split()) & set(params['subtypes'])))


def spec(declaration, record):
    if (not record or declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or 'variants' not in declaration):
        raise ValueError('typed version-two whole Oracle declaration required')
    params = declaration.get('params')
    expected = classify(record)
    if (type(params) is not dict or params != expected or type(params['subtypes']) is not list
            or type(params['outputs']) is not list):
        raise ValueError('exact typed reveal and output parameters required')
    bound = profiles()['sources'].get(record['name'])
    if not bound or bound['oracle'] != record:
        raise ValueError('independent released source pin required')
    return dict(copy.deepcopy(params), family=FAMILY, effect=FAMILY, keywords=[])


def validate_witnesses(oracle, registry):
    data = profiles()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name)
            if (not row or row['handler'] != pin['handler'] or row['kind'] != 'card' or oracle.get(name) != pin['oracle']
                    or oracle[name]['layout'] != 'normal' or row['handler'] == 'NULL'
                    or row['handler'] == 'card_dummy' and not (group == 'witnesses' and pin.get('reviewed_native_witness'))):
                raise ValueError('whole normal Oracle/handler pin changed: ' + name)
    for profile in data['sources'].values():
        model = classify(profile['oracle'])
        for name in profile['choices']['positive']:
            if name not in data['witnesses'] or not matches(oracle[name], model):
                raise ValueError('positive reveal witness must independently match: ' + name)
        for name in profile['choices']['negative']:
            if name not in data['witnesses'] or matches(oracle[name], model):
                raise ValueError('negative reveal witness must independently fail: ' + name)


def variants(params):
    modes = ['reveal', 'reveal_alternate', 'decline', 'wrong_only', 'empty_hand', 'opponent_hand_only', 'battlefield_only',
             'opposing_turn', 'atlas_decline', 'pending_snapshot', 'reenter_decline', 'reenter_reveal',
             'mana_first', 'mana_second', 'spend_first', 'spend_second', 'repeat_denied', 'untap_after_decline',
             'switch_colors', 'mana_snapshot']
    modes.append('kindred' if params['reveal_kind'] == 'creature_type' else 'nonbasic_typed')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


class RevealLandWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.entered = False
        self.land_tapped = False

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        super().receipts()
        if self.entered:
            self.observe('source', 'in_play', 1)
            self.observe('source', 'tapped', self.land_tapped)
        else:
            self.observe('source', 'in_hand', 1)

    def main(self, player):
        self.current = player
        self.emit('turn', player=player, value=0)

    def offered(self, names, predicate):
        self.emit('assert_offered_count', value=len(names))
        self.emit('assert_offered_selection_required', value=0)
        for name in sorted(set(names)):
            self.emit('assert_offered_legal_card', index=self.rows[name]['csvid'], value=int(predicate(self.o[name])))

    def land_choice(self, reveal, *, decline_entry=False):
        owner, csvid = self.owner, self.rows[self.name]['csvid']
        hand = list(self.zones['hand'][owner])
        remaining = hand.copy()
        remaining.remove(self.name)
        legal_reveals = [name for name in remaining if matches(self.o[name], self.p)]
        if reveal is not None and (decline_entry or reveal not in legal_reveals):
            raise ValueError('all scripted reveal picks must be legal by Oracle')
        self.emit('observe_search_filters', value=1)
        self.emit('require_can_activate', ref='atlas', value=1)
        self.emit('activate', ref='atlas', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 1)
        self.entities['atlas']['tapped'] = True
        cut = len(self.steps)
        if decline_entry:
            self.emit('choose_optional_zone', player=owner, value=csvid)
        else:
            self.emit('choose_zone_card', player=owner, value=csvid)
            if reveal is not None:
                self.emit('choose_zone_card', player=owner, value=self.rows[reveal]['csvid'])
                if owner == 0:
                    self.emit('choose_dialog', player=owner, value=0)
            elif remaining:
                # Safe optional decline also diagnoses an offer on a hand with
                # no qualifying cards. The final offered window must be Atlas.
                fallback = (legal_reveals or remaining)[0]
                self.emit('choose_optional_zone', player=owner, value=self.rows[fallback]['csvid'])
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_forced_zone_choices', value=0)
        if not decline_entry and legal_reveals:
            self.offered(remaining, lambda record: matches(record, self.p))
        else:
            self.offered(hand, lambda record: 'Land' in record['type_line'].partition(' — ')[0].split())
        self.emit('observe_search_filters', value=0)
        self.emit('refresh')
        if not decline_entry:
            self.zones['hand'][owner].remove(self.name)
            self.zones['battlefield'][owner].append(self.name)
            self.entered = True
            self.land_tapped = reveal is None
        self.receipts()
        return cut

    def produce(self, symbol):
        if not self.entered or self.land_tapped or symbol not in self.p['outputs']:
            raise ValueError('mana production requires an untapped land and printed choice')
        self.emit('require_can_activate', ref='source', value=1)
        self.emit('choose_color', player=self.owner, value=COLORS.index(symbol))
        self.emit('activate', ref='source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.mana[self.owner][COLORS.index(symbol)] += 1
        self.land_tapped = True
        self.receipts()

    def cannot_produce(self):
        self.emit('require_can_activate', ref='source', value=0)
        self.emit('activate', ref='source', value=0)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def spend(self, symbol, ref='spender'):
        from recipes import cost_from_oracle

        name = SPEND[symbol]
        cost = [0] * 6
        cost[COLORS.index(symbol)] = 1
        if cost != cost_from_oracle(self.o[name]) or self.mana[self.owner] != cost:
            raise ValueError('cast must spend exactly the land’s actual output')
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.mana[self.owner] = [0] * 6
        self.arrive(ref)
        if 'Haste' in self.o[name]['keywords']:
            self.entities[ref]['sick'] = False
        self.receipts()

    def bounce(self):
        from recipes import cost_from_oracle

        self.pool(self.owner, cost_from_oracle(self.o['Boomerang']))
        self.emit('refresh')
        self.emit('choose_target', player=self.owner, ref='source')
        self.emit('cast', ref='bounce', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        hand, battlefield = self.zones['hand'][self.owner], self.zones['battlefield'][self.owner]
        hand.remove('Boomerang')
        self.zones['graveyard'][self.owner].append('Boomerang')
        battlefield.remove(self.name)
        hand.append(self.name)
        self.mana[self.owner] = [0] * 6
        self.entered = self.land_tapped = False
        self.emit('bind_hand_copy', player=self.owner, value=self.rows[self.name]['csvid'], index=0, ref='source')
        self.receipts()

    def next_owner_main(self):
        for _ in range(2):
            if any(any(pool) for pool in self.mana):
                raise ValueError('actual untap requires both pools spent')
            self.emit('advance_turn')
            self.current = 1-self.current
            self.emit('untap_step', player=self.current)
            for card in self.entities.values():
                if card['zone'] == 'battlefield' and card['player'] == self.current:
                    card['tapped'] = card['sick'] = False
            if self.owner == self.current and self.entered:
                self.land_tapped = False
            self.main(self.current)
            self.receipts()


def _scenario(world, mode, choices):
    owner = world.owner
    enemy = 1-owner
    first, second, third = choices['positive']
    primary, secondary = world.p['outputs']
    world.add('Walking Atlas', 'atlas', owner)
    world.add(world.name, 'source', owner, 'hand')
    no_match = mode in ('wrong_only', 'empty_hand', 'opponent_hand_only', 'battlefield_only', 'atlas_decline')
    if mode in ('opponent_hand_only', 'battlefield_only'):
        world.add(first, 'remote', enemy if mode == 'opponent_hand_only' else owner,
                  'hand' if mode == 'opponent_hand_only' else 'battlefield')
    if mode not in ('empty_hand', 'atlas_decline'):
        for index, name in enumerate(choices['negative']):
            world.add(name, f'wrong_{index}', owner, 'hand')
    if not no_match:
        world.add(first, 'first', owner, 'hand')
        if mode in ('reveal_alternate', 'kindred', 'nonbasic_typed'):
            world.add(second, 'second', owner, 'hand')
        if mode in ('kindred', 'nonbasic_typed'):
            world.add(third, 'third', owner, 'hand')
    if mode in ('reenter_decline', 'reenter_reveal'):
        world.add('Boomerang', 'bounce', owner, 'hand')
    symbol = secondary if mode in ('mana_second', 'spend_second') else primary
    if mode in ('spend_first', 'spend_second', 'switch_colors'):
        world.add(SPEND[symbol], 'spender', owner, 'hand')
    if mode == 'switch_colors':
        world.add(SPEND[secondary], 'second_spender', owner, 'hand')
    for player in (0, 1):
        world.add('Island', f'lib_{player}', player, 'library')
        world.add('Plains', f'grave_{player}', player, 'graveyard')
        world.add('Mountain', f'exile_{player}', player, 'exile')

    world.main(enemy if mode == 'opposing_turn' else owner)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('refresh')
    world.receipts()
    decline = no_match or mode in ('decline', 'untap_after_decline', 'reenter_reveal')
    reveal = None if decline else second if mode == 'reveal_alternate' else third if mode in ('kindred', 'nonbasic_typed') else first
    pending_cut = world.land_choice(reveal, decline_entry=mode == 'atlas_decline')
    if mode == 'atlas_decline':
        return pending_cut
    if decline:
        world.cannot_produce()
    if mode in ('reenter_decline', 'reenter_reveal'):
        world.bounce()
        world.next_owner_main()
        world.land_choice(first if mode == 'reenter_reveal' else None)
        if mode == 'reenter_decline':
            world.cannot_produce()
        else:
            world.produce(primary)
    if mode == 'untap_after_decline':
        world.next_owner_main()
        world.produce(secondary)
    mana_cut = len(world.steps)
    if mode in ('mana_first', 'mana_second', 'spend_first', 'spend_second', 'repeat_denied', 'switch_colors', 'mana_snapshot', 'opposing_turn'):
        world.produce(symbol)
    if mode == 'repeat_denied':
        world.cannot_produce()
    if mode in ('spend_first', 'spend_second', 'switch_colors'):
        world.spend(symbol)
    if mode == 'switch_colors':
        world.next_owner_main()
        world.produce(secondary)
        world.spend(secondary, 'second_spender')
    return mana_cut if mode == 'mana_snapshot' else pending_cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    if not selected or len(set(selected)) != len(selected) or any(variant not in variants(params) for variant in selected):
        raise ValueError('explicit distinct reveal-land variants required')
    validate_witnesses(oracle, rows)
    choices = profiles()['sources'][name]['choices']
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = RevealLandWorld(name, params, int(owner), rows, oracle)
        cut = _scenario(world, mode, choices)
        steps = world.steps
        if mode in ('pending_snapshot', 'mana_snapshot'):
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + copy.deepcopy(tail)
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(
            name=f'HandRevealDualLandRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
            setup=world.setup, steps=steps, _recipe=dict(
                family=FAMILY, version=2, variant=variant, params=declaration['params'], oracle_text=declaration['oracle_text'],
                untested_components=['ordinary turn land drop UI separate from Walking Atlas placement',
                                     'additional tapped-entry effects, text-changing effects and multiplayer'])))
    return cases
