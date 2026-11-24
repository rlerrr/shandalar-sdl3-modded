"""Mana buyback paid during an actual cast, with full resolution receipts."""

import copy
import hashlib
import json
from pathlib import Path
import re

from block_stat_trigger_recipes import BlockStatBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'buyback_spell'
PROFILE_PATH = Path(__file__).with_name('data') / 'buyback_spell_wave8_profiles.json'
MANA = r'(?:\{[1-9]\})?(?:\{[WUBRG]\}){0,3}'
EFFECTS = {
    'Destroy target enchantment.': 'destroy_enchantment',
    "Return target permanent to its owner's hand.": 'bounce',
    'Target creature gets +2/+2 until end of turn.': 'pump',
    'Create a 1/1 black Rat creature token.': 'rat',
    'Target player discards a card.': 'discard',
    'Scry 3. (Look at the top three cards of your library, then put any number of them on the bottom and the rest on top in any order.)': 'scry',
    'Draw a card.': 'draw',
    'Destroy target artifact.': 'destroy_artifact',
    'Target creature gets +3/+0 until end of turn.': 'power_pump',
}
PERMANENT_TARGETS = {'destroy_enchantment', 'bounce', 'pump', 'destroy_artifact', 'power_pump'}
DESTRUCTION = {'destroy_enchantment', 'destroy_artifact'}
MODES = ('paid', 'ordinary', 'decline', 'short_fee', 'countered', 'recast', 'repeat_paid', 'pending_replay', 'wrong_base')
PAID = {'paid', 'countered', 'fizzle', 'recast', 'repeat_paid', 'pending_replay', 'expiry', 'offturn', 'empty_hand',
        'self_discard', 'scry_empty', 'scry_keep', 'indestructible', 'foreign_owner', 'token_subtype'}
LIBRARY = ('Unsummon', 'Counterspell', 'Lightning Bolt', 'Negate', 'Naturalize', 'Murder', 'Giant Growth')


def profiles():
    return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(record):
    name = record.get('name', '')
    wording = r'Buyback (' + MANA + r') \(You may pay an additional \1 as you cast this spell\. If you do, put this card into your hand as it resolves\.\)\n(.+)'
    match = re.fullmatch(wording, record.get('oracle_text', ''))
    base = record.get('mana_cost', '')
    if (not match or not match[1] or not base or not re.fullmatch(MANA, base) or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or record.get('type_line') not in ('Instant', 'Sorcery')):
        raise ValueError('complete ordinary spell and mana buyback clause required')
    effect = EFFECTS.get(match[2])
    if match[2] == f'{name} deals 2 damage to each creature and each player.':
        effect = 'symmetric_damage'
    keys = ['Buyback', 'Scry'] if effect == 'scry' else ['Buyback']
    if not effect or record.get('keywords') != keys or record.get('colors') != sorted(set(re.findall(r'\{([WUBRG])\}', base))):
        raise ValueError('exact buyback spell effect and printed keywords required')
    return dict(mana=base, fee=match[1], effect=effect, instant=record['type_line'] == 'Instant')


def variants(params):
    effect = params['effect']
    modes = list(MODES)
    if effect in PERMANENT_TARGETS:
        modes += ['fizzle', 'cancel_target']
    if effect != 'bounce' and effect in PERMANENT_TARGETS:
        modes.append('wrong_type')
    if effect in DESTRUCTION:
        modes.append('indestructible')
    if effect in ('power_pump', 'pump'):
        modes.append('expiry')
    if effect == 'bounce':
        modes.append('foreign_owner')
    if effect == 'discard':
        modes += ['short_color', 'empty_hand', 'self_discard']
    if effect == 'scry':
        modes += ['scry_empty', 'scry_keep']
    if effect == 'rat':
        modes.append('token_subtype')
    if params['instant']:
        modes.append('offturn')
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes if mode != 'scry_keep' or owner == 0]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not pin or pin['oracle'] != record or pin['params'] != params or declaration.get('oracle_text') != record['oracle_text']
            or declaration.get('params') != params):
        raise ValueError('complete pinned Oracle and mana buyback parameters required')
    choices = declaration.get('variants')
    if not isinstance(choices, list) or not choices or any(type(choice) is not str for choice in choices):
        raise ValueError('explicit buyback variants required')
    if len(set(choices)) != len(choices) or not set(choices) <= set(variants(params)):
        raise ValueError('unique reviewed buyback variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unimplemented = json.loads((PROFILE_PATH.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    pending = {e['csvid'] for e in unimplemented.values()}
    for name, pin in (data['witnesses'] | data['sources']).items():
        row = rows.get(name, {})
        if oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card':
            raise ValueError('buyback source or witness changed: ' + name)
        if row['csvid'] in pending or 'dummy' in row['handler']:
            if name not in data['static_characteristics_only_witnesses']:
                raise ValueError('buyback action witness must be implemented: ' + name)
    token = data['token']
    if rows.get('Rat') != token['registry'] or oracle.get('Rat') != token['oracle']:
        raise ValueError('pinned native Rat token identity required')
    root = Path(__file__).resolve().parents[4]
    for name, pin in data['sources'].items():
        source = root / pin['compiled_source']['path']
        if hashlib.sha256(source.read_bytes()).hexdigest() != pin['compiled_source']['sha256']:
            raise ValueError('buyback compiled source changed: ' + name)


class BuybackBoard(BlockStatBoard):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import card, cost_from_oracle, observe, step

        self.card, self.step, self.observe, self.cost = card, step, observe, cost_from_oracle
        self.name, self.csvid, self.declaration, self.p, self.rows, self.oracle = name, csvid, declaration, params, rows, oracle
        self.mode, owner = variant.rsplit('_', 1)
        self.owner = int(owner)
        self.effect = params['effect']
        self.steps, self.setup, self.entities, self.base = [], [], {}, {}
        self.tokens, self.bound = [], set()
        self.boundary = self.discard_nondeterministic = False
        self.life, self.mana, self.pools = [20, 20], [0, 0], [[0] * 6, [0] * 6]
        self.zones = []
        self.targets = self.effect in PERMANENT_TARGETS
        self.twice = self.mode in ('recast', 'repeat_paid')
        self.response = 'Counterspell' if self.mode == 'countered' else self.late_reply() if self.mode == 'fizzle' else None
        self.discard_player = int(self.mode != 'self_discard')
        self.stock()
        self.add(name, 'source', 0, 'hand')
        self.set_board()
        self.emit('turn', player=int(self.mode == 'offturn'), value=0)
        self.emit('present_ai_dialogs', value=1)
        self.payments()
        self.emit('refresh')

    def add(self, name, ref, player=0, zone='battlefield', state=None):
        if name != 'Pack Rat':
            super().add(name, ref, player, zone, state)
            return
        # Pack Rat counts itself, and its dynamic value becomes two when Lab
        # Rats makes a Rat. The accompanying non-Rat does not affect the count.
        self.setup.append(self.card(name, ref, player, zone))
        self.entities[ref] = dict(name=name, player=player, owner=player, zone=zone, creature=True, power=1,
                                  toughness=1, counters=0, boost=0, damage=0, tapped=False, token=False)

    def stock(self):
        for player in (0, 1):
            hand = ['Forest', 'Plains']
            if self.effect == 'discard' and player == self.discard_player:
                # The unassisted AI discard path can choose either identical card.
                if (player ^ self.owner) == 1:
                    hand = ['Forest', 'Forest']
                if self.mode == 'empty_hand':
                    hand = []
            library = [] if player == 0 and self.mode == 'scry_empty' else list(LIBRARY)
            zones = dict(hand=hand, graveyard=['Island'], exile=['Mountain'], library=library)
            self.zones.append(zones)
            for zone, names in zones.items():
                self.setup.extend(self.card(name, f'stock_{zone}_{player}_{index}', player, zone) for index, name in enumerate(names))

    def late_reply(self):
        return 'Disenchant' if self.effect in DESTRUCTION else 'Unsummon'

    def set_board(self):
        if self.targets and self.mode != 'wrong_type':
            target_name = {'destroy_enchantment': 'Glorious Anthem', 'destroy_artifact': 'Ornithopter'}.get(self.effect, 'Hill Giant')
            if self.mode == 'indestructible':
                target_name = 'Thassa, God of the Sea' if self.effect == 'destroy_enchantment' else 'Darksteel Relic'
            if self.mode == 'foreign_owner':
                target_name = 'Hill Giant'
                self.add('Act of Treason', 'steal', 0, 'hand')
            self.add(target_name, 'target_0', 1)
            if self.twice:
                self.add(target_name, 'target_1', 0)
        elif self.mode == 'wrong_type':
            self.add('Hill Giant' if self.effect in DESTRUCTION else 'Glorious Anthem', 'excluded', 1)
        if self.effect == 'symmetric_damage':
            for player in (0, 1):
                self.add('Grizzly Bears', f'bear_{player}', player)
                self.add('Hill Giant', f'giant_{player}', player)
                self.add('Howling Mine', f'idle_{player}', player)
        if self.mode == 'token_subtype':
            self.add('Pack Rat', 'pack')
            self.add('Grizzly Bears', 'non_rat')
        if self.response:
            self.add(self.response, 'reply', 1, 'hand')

    def payments(self):
        base = self.cost(dict(mana_cost=self.p['mana']))
        fee = self.cost(dict(mana_cost=self.p['fee']))
        count = 2 if self.twice else 1
        extra = fee.copy()
        if self.mode in ('ordinary', 'wrong_type'):
            extra = [0] * 6
        if self.mode == 'short_fee':
            index = next(i for i, value in enumerate(extra) if value)
            extra[index] -= 1
        if self.mode == 'short_color':
            extra[0] += extra[1]
            extra[1] = 0
        pool = [count * (a + b) for a, b in zip(base, extra)]
        if self.mode == 'wrong_base':
            pool = [sum(base) + sum(fee)] + [0] * 5
        if self.mode == 'foreign_owner':
            pool = [a + b for a, b in zip(pool, self.cost(self.oracle['Act of Treason']))]
        self.set_pool(0, pool)
        self.set_pool(1, self.cost(self.oracle[self.response]) if self.response else [0] * 6)

    def show_pools(self):
        for player, pool in enumerate(self.pools):
            for color, mana in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=mana)

    def begin_cast(self, index, buy):
        self.emit('can_cast', ref='source', value=int(self.mode != 'wrong_type'))
        offer_buyback = self.mode not in ('ordinary', 'short_fee', 'short_color', 'wrong_base', 'wrong_type')
        # Seething Anger's legacy callback reaches the buyback menu without
        # offering its required target. Retain the target intent as a receipt
        # checked by assert_unused_choices, independent of the payment.
        if self.effect == 'power_pump' and offer_buyback:
            self.emit('choose_dialog', player=0, value=int(not buy))
        if self.mode == 'cancel_target':
            self.emit('choose_cancel_target', player=0)
        elif self.mode not in ('wrong_base', 'wrong_type'):
            if self.targets:
                self.emit('choose_target', player=0, ref=f'target_{index}')
            elif self.effect == 'discard':
                self.emit('choose_player', player=0, value=self.discard_player)
            if offer_buyback and self.effect != 'power_pump':
                self.emit('choose_dialog', player=0, value=int(not buy))
        allowed = self.mode not in ('wrong_base', 'wrong_type', 'cancel_target')
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(allowed))
        self.emit('assert_card', ref='source', property='in_hand', value=int(not allowed))
        if allowed:
            self.spend(self.p['mana'])
            if buy:
                self.spend(self.p['fee'])
        self.show_pools()
        return allowed

    def plan_effect(self):
        if self.effect == 'scry':
            self.emit('record_library', player=0)
            deck = self.zones[0]['library']
            if self.mode == 'scry_keep':
                self.emit('choose_zone', player=0, value=-1)
                self.emit('choose_zone_card', player=0, value=self.rows[deck[2]]['csvid'])
                self.emit('choose_zone', player=0, value=-1)
                deck.insert(0, deck.pop(2))
            else:
                window = deck[:3]
                for name in window:
                    self.emit('choose_zone_card', player=0, value=self.rows[name]['csvid'])
                deck[:] = deck[3:] + window
        if self.effect == 'discard' and self.zones[self.discard_player]['hand']:
            target = self.discard_player
            name = self.zones[target]['hand'][0]
            if target ^ self.owner:
                # The legacy AI announces the identity after choosing it.
                self.emit('choose_dialog', player=target, value=0)
            else:
                self.emit('choose_zone_card', player=target, value=self.rows[name]['csvid'])
            self.zones[target]['hand'].remove(name)
            self.zones[target]['graveyard'].append(name)

    def token(self):
        ref = f'rat_{len(self.tokens)}'
        self.tokens.append(ref)
        self.entities[ref] = dict(name='Rat', owner=0, player=0, zone='battlefield', creature=True, power=1, toughness=1,
                                  boost=0, counters=0, damage=0, tapped=False, token=True)
        if 'pack' in self.entities:
            self.entities['pack']['power'] += 1
            self.entities['pack']['toughness'] += 1

    def apply_effect(self, ref):
        if self.effect in DESTRUCTION and self.mode != 'indestructible':
            self.move(ref, 'graveyard')
        elif self.effect == 'bounce':
            self.move(ref, 'hand')
            self.emit('bind_hand', ref=ref, player=self.entities[ref]['owner'], value=self.rows[self.entities[ref]['name']]['csvid'])
        elif self.effect in ('pump', 'power_pump'):
            amount = 2 if self.effect == 'pump' else 3
            self.entities[ref]['power'] += amount
            if self.effect == 'pump':
                self.entities[ref]['toughness'] += amount
        elif self.effect == 'symmetric_damage':
            for player in (0, 1):
                self.life[player] -= 2
                for target in (f'bear_{player}', f'giant_{player}'):
                    entity = self.entities[target]
                    if entity['zone'] == 'battlefield':
                        entity['damage'] += 2
                        if entity['damage'] >= entity['toughness']:
                            self.move(target, 'graveyard')
        elif self.effect == 'rat':
            self.token()
        elif self.effect == 'draw':
            self.draw(0, 1)
        elif self.effect == 'scry':
            self.emit('assert_scry_result', player=0, value=3)

    def resolve_cast(self, index, buy):
        ref = f'target_{index}'
        if self.response:
            self.emit('announce_spell', ref='source')
            if self.mode == 'fizzle':
                self.emit('choose_target', player=1, ref=ref)
            self.emit('cast_response', ref='reply', value=1)
            self.spend(self.oracle[self.response]['mana_cost'], player=1)
            self.emit('assert_stack', value=2)
            self.emit('resolve_response')
            self.move('reply', 'graveyard')
            if self.mode == 'fizzle':
                self.move(ref, 'graveyard' if self.effect in DESTRUCTION else 'hand')
        else:
            self.plan_effect()
            self.emit('resolve')
            self.apply_effect(ref)
        self.emit('refresh')
        returns = buy and not self.response
        if returns:
            self.emit('bind_hand', ref='source', player=0, value=self.csvid)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        else:
            self.move('source', 'graveyard')
        self.final_receipts()

    def final_receipts(self):
        # Count actual native tokens by identity, then inspect the printed traits.
        if self.effect == 'rat':
            token_id = self.rows['Rat']['csvid']
            self.emit('assert_zone_multiplicity', player=0, zone='battlefield', index=token_id, value=len(self.tokens))
            self.emit('assert_zone_multiplicity', player=1, zone='battlefield', index=token_id, value=0)
            if self.tokens:
                self.emit('assert_matching_permanents', player=0, index=token_id, property='token', value=1)
                self.emit('assert_matching_permanents', player=0, index=token_id, property='color', value=2)
        # The shared generic token receipts add a Zombie assertion. Here the
        # actual subtype is Rat, so emit the creature receipts without that branch.
        for token in self.tokens:
            self.entities[token]['token'] = False
        receipt_start = len(self.steps)
        self.receipts()
        if self.effect == 'destroy_enchantment' and self.mode == 'indestructible':
            # Thassa contributes the sole blue devotion, below the five
            # needed to be a creature. Check actual creature ineligibility.
            self.steps[receipt_start:] = [s for s in self.steps[receipt_start:]
                if not (s['op'] == 'assert_card' and s.get('ref') == 'target_0' and s['property'] in ('power', 'toughness'))]
            self.emit('can_target_creature', ref='source', other='target_0', value=0)
        for token in self.tokens:
            self.entities[token]['token'] = True

    def run(self):
        if self.mode == 'token_subtype':
            for prop in ('power', 'toughness'):
                self.emit('assert_card', ref='pack', property=prop, value=1)
        if self.mode == 'foreign_owner':
            self.paid('steal', target='target_0')
            self.entities['target_0'].update(player=0, tapped=False)
            self.emit('assert_zone', player=0, zone='battlefield', value=1)
            self.emit('assert_zone', player=1, zone='battlefield', value=0)
            self.emit('bind_permanent', ref='target_0', player=0, value=self.rows['Hill Giant']['csvid'])
        cut = None
        buy = self.mode in PAID
        if self.begin_cast(0, buy):
            if self.mode == 'pending_replay':
                cut = len(self.steps)
            self.resolve_cast(0, buy)
            if self.twice:
                again = self.mode == 'repeat_paid'
                self.begin_cast(1, again)
                self.resolve_cast(1, again)
            if self.mode == 'expiry':
                self.emit('cleanup')
                self.emit('refresh')
                self.reset_stats()
                self.final_receipts()
        else:
            self.final_receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'BuybackSpellRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=copy.deepcopy(self.declaration['params']), oracle_text=self.declaration['oracle_text'],
                    untested_components=['Compounded buyback discounts and AI strategy are outside the fixed printed-payment model.']))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params)):
        raise ValueError('unique reviewed buyback variants required')
    if rows[name]['csvid'] != csvid:
        raise ValueError('buyback source identity changed')
    return [BuybackBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
