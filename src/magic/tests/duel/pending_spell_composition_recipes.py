"""Real pending spell compositions with exact printed costs and clash choices."""
import copy
import hashlib
import json
from pathlib import Path

from clash_whole_cards_recipes import CLASH, REMINDER, ClashBoard
from combat_value_trigger_recipes import _mirror

FAMILY = 'pending_spell_composition'
PROFILE = Path(__file__).with_name('data') / 'pending_spell_composition_wave8_profiles.json'
TAIL = REMINDER + ')'
CLASH_CLAUSES = {
    'delayed': "Counter target spell. " + CLASH + " If you win, at the beginning of your next main phase, you may add an amount of {C} equal to that spell's mana value.",
    'enchantments': 'Destroy target enchantment. ' + CLASH + ' If you win, destroy all enchantments your opponents control.',
    'damage_x': '{name} deals X damage to any target. ' + CLASH + " If you win, return {name} to its owner's hand.",
}
MODES = {
    'delayed': ('win', 'tie', 'loss', 'human_bottom', 'snapshot', 'optional_decline', 'own_spell', 'no_spell', 'uncounterable', 'x_value'),
    'enchantments': ('win', 'tie', 'loss', 'human_bottom', 'snapshot', 'own_target', 'fizzle', 'wrong_type'),
    'damage_x': ('win', 'tie', 'loss', 'human_bottom', 'snapshot', 'x_zero', 'creature', 'walker', 'own_player', 'fizzle', 'recast', 'short_color'),
}


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal pending spell composition required')
    text = record['oracle_text']
    for kind, clause in CLASH_CLAUSES.items():
        if text == clause.replace('{name}', record['name']) + TAIL:
            spell_type = 'Sorcery' if kind == 'damage_x' else 'Instant'
            if record['type_line'] != spell_type or record['keywords'] != ['Clash']:
                raise ValueError('unrecognized printed Clash composition')
            return dict(effect=kind, mana=record['mana_cost'])
    raise ValueError('unsupported whole pending spell composition')


def variants(params):
    return [f'{mode}_{seat}' for mode in MODES[params['effect']] for seat in (0, 1)]


def spec(declaration, record):
    params = parameters(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record or pin['params'] != params
            or declaration.get('oracle_text') != record['oracle_text'] or declaration.get('params') != params):
        raise ValueError('complete pinned pending spell record required')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(type(x) is not str for x in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('unique reviewed pending spell variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    pinned = profiles()
    data = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {record['csvid'] for record in data['cases'].values()}
    root = Path(__file__).resolve().parents[4]
    for name, pin in (pinned['sources'] | pinned['witnesses']).items():
        row = rows.get(name, {})
        if (oracle.get(name) != pin['oracle'] or row != pin['registry'] or row.get('kind') != 'card'
                or pin['oracle']['layout'] != 'normal'):
            raise ValueError('pending spell registry or Oracle witness changed: ' + name)
        if name not in pinned['characteristic_only'] and (row['csvid'] in unimplemented or 'dummy' in row['handler']):
            raise ValueError('pending spell action witness unimplemented: ' + name)
        if ('compiled' in pin
                and hashlib.sha256((root / pin['compiled']['path']).read_bytes()).hexdigest() != pin['compiled']['sha256']):
            raise ValueError('compiled pending spell source changed: ' + name)

class PendingBoard(ClashBoard):
    """Reuse actual clash placement and tracked zone/cost receipts."""

    def witnesses(self):
        effect, mode = self.effect, self.mode
        if effect == 'delayed' and mode != 'no_spell':
            self.parent_name = {'uncounterable': 'Thrun, the Last Troll', 'own_spell': 'Crater Elemental',
                                'x_value': 'Blaze'}.get(mode, 'Lightning Bolt')
            self.parent_player = 0 if mode == 'own_spell' else 1
            self.add(self.parent_name, 'parent', self.parent_player, 'hand')
            if mode == 'own_spell':
                self.add('Grizzly Bears', 'attacker')
        elif effect == 'enchantments':
            self.add('Howling Mine', 'artifact', 1)
            if mode != 'wrong_type':
                self.add('Glorious Anthem', 'target', 0 if mode == 'own_target' else 1)
                self.add('Fervor', 'their_enchantment', 1)
                self.add('Levitation', 'their_other', 1)
                self.add('Bad Moon', 'our_enchantment', 0)
            if mode == 'fizzle':
                self.add('Disenchant', 'reply', 1, 'hand')
        elif effect == 'damage_x':
            if mode in ('creature', 'fizzle', 'recast'):
                self.add('Crater Elemental', 'target', 1)
            if mode == 'walker':
                self.add('Jace Beleren', 'target', 1)
                self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
            if mode == 'fizzle':
                self.add('Unsummon', 'reply', 1, 'hand')

    def pay(self, payment, player=0):
        self.pools[player] = [available - used for available, used in zip(self.pools[player], payment)]
        self.mana[player] = sum(self.pools[player])
        if min(self.pools[player]) < 0:
            raise ValueError('reviewed payment exceeds actual pool')

    def fund(self, player, payment):
        self.set_pool(player, payment)
        self.emit('refresh')

    def normal_start(self, current=0):
        self.emit('turn', player=current, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.set_pool(player, [0]*6)
        self.emit('refresh')

    def pending(self, ref, *, response=False, allowed=True):
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        if allowed:
            self.emit('assert_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_stack', value=(2 if allowed else 1) if response else int(allowed))

    def parent_cast(self, x=0):
        name, player = self.parent_name, self.parent_player
        cost = self.cost(dict(mana_cost=self.oracle[name]['mana_cost'].replace('{X}', '')))
        cost[0] += x
        self.fund(player, cost)
        if name == 'Blaze':
            self.emit('choose_x', player=player, value=x)
        if name in ('Lightning Bolt', 'Blaze'):
            self.emit('choose_player', player=player, value=1-player)
        self.pending('parent')
        self.pay(cost, player)
        self.emit('announce_spell', ref='parent')
        self.emit('assert_unused_choices', value=0)

    def parent_effect(self, countered, damage=3):
        name, player = self.parent_name, self.parent_player
        creature = self.entities['parent']['creature']
        self.move('parent', 'battlefield' if creature and not countered else 'graveyard')
        if not countered and name in ('Lightning Bolt', 'Blaze'):
            self.life[1-player] -= damage

    def round(self):
        return self.queue_round([z['library'] for z in self.zones], self.mode == 'human_bottom')

    def invalidated_round(self):
        # A failed all-targets check must leave these lawful but unused
        # acknowledgements/placements untouched.
        _, choices = self.queue_round([z['library'][:] for z in self.zones], False)
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=choices)

    def delayed(self):
        mode = self.mode
        own = mode == 'own_spell'
        self.normal_start(0 if own else 1)
        self.fund(0, self.cost(self.oracle[self.name]))
        if mode == 'no_spell':
            self.emit('can_cast', ref='source', value=0)
            self.pending('source', allowed=False)
            return None
        x = 3 if mode == 'x_value' else 0
        self.parent_cast(x=x)
        if own:
            self.fund(0, self.cost(self.oracle[self.name]))
        self.pending('source', response=True)
        self.spend(self.p['mana'])
        self.pools_receipt()
        cut = len(self.steps) if mode == 'snapshot' else None
        won, _ = self.round()
        self.emit('resolve_response')
        self.emit('assert_unused_choices', value=0)
        self.move('source', 'graveyard')
        self.parent_effect(mode != 'uncounterable')
        # Empty pools until the actual next main transition. The old optional
        # chooser, if absent, is observed without forcing a fake menu.
        self.pools_receipt()
        if own:
            self.emit('begin_attackers')
            self.emit('attack', ref='attacker', value=1)
            self.entities['attacker']['tapped'] = True
            self.life[1] -= self.power('attacker')
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
        if won:
            self.emit('choose_dialog', player=0, value=int(mode == 'optional_decline'))
        self.emit('end_combat' if own else 'advance_turn')
        self.emit('assert_unused_choices', value=0)
        if won and mode != 'optional_decline':
            reward = int(self.oracle[self.parent_name]['cmc']) + x
            self.mana[0] = self.pools[0][0] = reward
        self.pools_receipt()
        self.emit('assert_mana', player=1, color=0, value=0)
        return cut

    def destroy(self, ref):
        self.move(ref, 'graveyard')

    def spring(self):
        self.normal_start()
        self.fund(0, self.cost(self.oracle[self.name]))
        if self.mode == 'wrong_type':
            self.emit('can_cast', ref='source', value=0)
            self.pending('source', allowed=False)
            return None
        if self.mode == 'fizzle':
            self.fund(1, self.cost(self.oracle['Disenchant']))
        self.emit('choose_target', player=0, ref='target')
        self.pending('source')
        self.spend(self.p['mana'])
        self.pools_receipt()
        cut = len(self.steps) if self.mode == 'snapshot' else None
        if self.mode == 'fizzle':
            self.emit('announce_spell', ref='source')
            self.emit('choose_target', player=1, ref='target')
            self.pending('reply', response=True)
            self.spend(self.oracle['Disenchant']['mana_cost'], 1)
            self.invalidated_round()
            self.destroy('reply')
            self.destroy('target')
        else:
            won, _ = self.round()
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.destroy('target')
            if won:
                for ref in ('their_enchantment', 'their_other'):
                    self.destroy(ref)
        self.destroy('source')
        self.emit('refresh')
        return cut

    def damage_spell(self, x, target, recipient):
        price = self.cost(dict(mana_cost=self.p['mana'].replace('{X}', '')))
        price[0] += x
        self.fund(0, price)
        self.emit('choose_x', player=0, value=x)
        if target:
            self.emit('choose_target', player=0, ref='target')
        else:
            self.emit('choose_player', player=0, value=recipient)
        self.pending('source')
        self.pay(price)
        self.pools_receipt()
        cut = len(self.steps) if self.mode == 'snapshot' else None
        if self.mode == 'fizzle':
            self.emit('announce_spell', ref='source')
            self.emit('choose_target', player=1, ref='target')
            self.pending('reply', response=True)
            self.spend(self.oracle['Unsummon']['mana_cost'], 1)
            self.invalidated_round()
            self.destroy('reply')
            self.move('target', 'hand')
            returns = False
        else:
            won, _ = self.round()
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            returns = won
            if not target:
                self.life[recipient] -= x
            elif self.loyalty is not None:
                self.loyalty -= x
            else:
                self.entities['target']['damage'] += x
        if returns:
            self.emit('bind_hand', ref='source', player=0, value=self.csvid)
            self.emit('assert_card', ref='source', property='in_hand', value=1)
        else:
            self.destroy('source')
        self.emit('refresh')
        if self.loyalty is not None:
            self.emit('assert_counter', ref='target', counter_kind='loyalty', value=self.loyalty)
        return cut

    def revenge(self):
        self.normal_start()
        if self.mode == 'short_color':
            self.fund(0, [8,0,0,0,1,0])
            self.pending('source', allowed=False)
            return None
        if self.mode == 'fizzle':
            self.fund(1, self.cost(self.oracle['Unsummon']))
        target = self.mode in ('creature', 'fizzle', 'walker', 'recast')
        x = 0 if self.mode == 'x_zero' else 2 if self.mode == 'walker' else 3
        cut = self.damage_spell(x, target, int(self.mode != 'own_player'))
        if self.mode == 'recast':
            self.receipts()
            self.damage_spell(2, False, 1)
        return cut

    def run(self):
        operation = {'delayed': self.delayed, 'enchantments': self.spring, 'damage_x': self.revenge}[self.effect]
        cut = operation()
        self.receipts()
        if cut is not None:
            tail = self.steps[cut:]
            self.steps[cut:cut] = [self.step('capture')]
            self.steps += [self.step('record_state'), self.step('ai_roundtrip'), self.step('restore')]
            self.steps += copy.deepcopy(tail) + [self.step('assert_recorded_state')]
        variant = f'{self.mode}_{self.owner}'
        case = dict(name=f'PendingSpellCompositionRecipe_{self.csvid}_{variant}', card=self.name, fixture='empty_main',
                    seed=123, setup=self.setup, steps=self.steps, _recipe=dict(family=FAMILY, version=2, variant=variant,
                    params=copy.deepcopy(self.d['params']), oracle_text=self.d['oracle_text']))
        if self.owner:
            _mirror(case)
        return case


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if (not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(params))
            or rows[name]['csvid'] != csvid):
        raise ValueError('valid pending spell source and reviewed variants required')
    return [PendingBoard(name, csvid, declaration, params, variant, rows, oracle).run() for variant in selected]
