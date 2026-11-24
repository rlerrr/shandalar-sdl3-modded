"""Fixed whole Dash creatures using actual costs, combat and end-turn triggers."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact, pin


FAMILY = 'dash_creature'
DATA = Path(__file__).parent / 'data/dash_creature_wave8_profiles.json'
DASH = re.compile(r"Dash (\{[12]\}\{[BR]\}) \(You may cast this spell for its dash cost\. If you do, it gains haste, "
                  r"and it's returned from the battlefield to its owner's hand at the beginning of the next end step\.\)")
MODES = ('dash_exact', 'normal_exact', 'dash_menu', 'normal_menu', 'cancel', 'unfunded', 'wrong_color',
         'dash_combat', 'normal_fresh_attack', 'countered_dash', 'destroyed', 'blink', 'donated',
         'recast_dash', 'recast_normal', 'pending_replay', 'return_replay')
SUPPORT = {'blink': 'Cloudshift', 'countered_dash': 'Essence Scatter', 'donated': 'Bazaar Trader'}
LIBRARY = ('Island', 'Hill Giant', 'Mountain', 'Forest', 'Grizzly Bears', 'Swamp')
WITNESSES = set(LIBRARY) | set(SUPPORT.values()) | {'Plains'}
ZERO = [0] * 6


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or record.get('keywords') != ['Dash']
            or not re.fullmatch(r'(\{[123]\}\{[BR]\}|\{R\}\{R\})', record.get('mana_cost', ''))
            or any(not re.fullmatch(r'[1-9]', record.get(prop, '')) for prop in ('power', 'toughness'))
            or not (match := DASH.fullmatch(record.get('oracle_text', '')))):
        raise ValueError('fixed-cost normal creature with the complete sole Dash clause required')
    regular = cost_from_oracle(record)
    alternate = cost_from_oracle({'mana_cost': match[1]})
    if {i for i in range(1, 6) if alternate[i]} != {i for i in range(1, 6) if regular[i]} or alternate == regular:
        raise ValueError('two distinct reviewed costs sharing the same mana color required')
    return dict(regular=regular, alternate=alternate, power=int(record['power']), toughness=int(record['toughness']))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')
            or not exact(pins()['sources'].get(record['name']), pin(record))):
        raise ValueError('exact Dash source and recipe version required')
    params = classify(record)
    if not exact(declaration.get('params'), params):
        raise ValueError('Dash parameters must derive from Oracle')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed Dash variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(data['handlers']) != set(records):
        raise ValueError('complete Dash fixture identity and handler pins required')
    implemented = set(data['sources']) | set(SUPPORT.values())
    for name, expected in records.items():
        row = rows[name]
        if not exact(pin(oracle[name]), expected) or row['handler'] != data['handlers'][name]:
            raise ValueError(f'Oracle or handler changed for {name}')
        if name in implemented and (row['kind'] != 'card' or row['handler'] in ('card_dummy', '0', 'NULL')):
            raise ValueError(f'implemented primary required for {name}')


def can_pay(pool, cost):
    return sum(pool) >= sum(cost) and all(p >= c for p, c in zip(pool[1:], cost[1:]))


class DashCase:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        from recipes import cost_from_oracle, step

        mode, seat = variant.rsplit('_', 1)
        self.mode, self.owner, self.variant = mode, int(seat), variant
        self.other = 1 - self.owner
        self.name, self.csvid, self.declaration, self.rows = name, csvid, declaration, rows
        self.regular, self.alternate = params['regular'], params['alternate']
        self.power, self.toughness = params['power'], params['toughness']
        self.use_dash = mode not in ('normal_exact', 'normal_menu', 'normal_fresh_attack')
        self.setup, self.steps = [], []
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.life, self.mana = [20, 20], [list(ZERO), list(ZERO)]
        self.source_controller, self.source_alive, self.source_tapped = self.owner, False, False
        self.trader_tapped = False
        self.add(name, 'source', self.owner, 'hand')
        self.mana[self.owner] = self.setup_budget()
        self.support = SUPPORT.get(mode)
        if self.support:
            who = self.other if mode == 'countered_dash' else self.owner
            self.add(self.support, 'support', who, 'battlefield' if mode == 'donated' else 'hand')
            if mode != 'donated':
                self.support_fee = cost_from_oracle(oracle[self.support])
                self.mana[who] = [have + fee for have, fee in zip(self.mana[who], self.support_fee)]
        for player in (0, 1):
            self.add('Mountain', f'grave_{player}', player, 'graveyard')
            self.add('Plains', f'exile_{player}', player, 'exile')
            for index, witness in enumerate(LIBRARY):
                self.add(witness, f'lib_{player}_{index}', player, 'library')
        self.names = sorted({row['name'] for row in self.setup})
        self.before = [step('turn', player=self.owner, value=0), step('present_ai_dialogs', value=1)]
        self.before.extend(step('set_mana', player=who, color=color, value=amount)
                           for who in (0, 1) for color, amount in enumerate(self.mana[who]))
        self.before.append(step('refresh'))
        self.snapshot = 0

    def setup_budget(self):
        if self.mode == 'unfunded':
            return list(ZERO)
        if self.mode == 'wrong_color':
            return [sum(self.alternate), 0, 0, 0, 0, 0]
        if self.mode in ('dash_menu', 'normal_menu', 'cancel'):
            return [max(a, b) for a, b in zip(self.regular, self.alternate)]
        return list(self.alternate if self.use_dash else self.regular)

    def add(self, name, ref, who, zone):
        from recipes import card

        self.setup.append(card(name, ref, who, zone))
        self.zones[zone][who].append(name)

    def move(self, name, who, start, finish, recipient=None):
        self.zones[start][who].remove(name)
        self.zones[finish][who if recipient is None else recipient].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def spend(self, who, fee):
        # Reviewed fixtures supply the generic portion explicitly in color 0.
        self.mana[who] = [have - due for have, due in zip(self.mana[who], fee)]
        if any(value < 0 for value in self.mana[who]):
            raise ValueError('fixture must fund the exact payment vector')

    def receipt(self, stack=0, pending=None):
        self.emit('assert_stack', value=stack)
        self.emit('assert_choices_consumed')
        if self.source_alive:
            for prop, value in [('in_play', 1), ('power', self.power), ('toughness', self.toughness), ('tapped', int(self.source_tapped))]:
                self.emit('assert_card', ref='source', property=prop, value=value)
        if self.mode == 'donated':
            self.emit('assert_card', ref='support', property='tapped', value=int(self.trader_tapped))
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=amount)
            for zone, seats in self.zones.items():
                cards = seats[who]
                if zone == 'hand' and pending is not None and pending[0] == who:
                    # The legacy zone count includes the physical slot while
                    # the independent in_hand observation excludes the spell.
                    cards = cards + [pending[1]]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library':
                    for index, witness in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[witness]['csvid'])
                else:
                    counts = Counter(cards)
                    for witness in self.names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[witness]['csvid'], value=counts[witness])

    def cast_source(self, dash, *, cancel=False):
        pool = self.mana[self.owner]
        regular, alternate = can_pay(pool, self.regular), can_pay(pool, self.alternate)
        if regular and alternate:
            self.emit('choose_dialog', player=self.owner, value=2 if cancel else int(not dash))
        if cancel and not (regular and alternate):
            raise ValueError('show the actual cancel option only when both modes are offered')
        allowed = (alternate if dash else regular) and not cancel
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_choices_consumed')
        if not allowed:
            self.emit('assert_card', ref='source', property='in_hand', value=1)
            self.receipt()
            return False
        self.emit('require_card', ref='source', property='in_hand', value=0)
        self.spend(self.owner, self.alternate if dash else self.regular)
        self.zones['hand'][self.owner].remove(self.name)
        self.receipt(1, pending=(self.owner, self.name))
        return True

    def resolve_source(self):
        self.emit('resolve')
        self.emit('refresh')
        self.zones['battlefield'][self.owner].append(self.name)
        self.source_alive, self.source_tapped, self.source_controller = True, False, self.owner
        self.receipt()

    def counter(self):
        self.emit('announce_spell', ref='source')
        self.emit('cast_response', ref='support', value=1)
        self.emit('assert_stack', value=2)
        self.spend(self.other, self.support_fee)
        self.move(self.support, self.other, 'hand', 'graveyard')
        self.zones['graveyard'][self.owner].append(self.name)
        self.emit('resolve_response')
        self.emit('refresh')
        self.receipt()

    def combat(self, legal):
        self.emit('begin_attackers')
        self.emit('attack', ref='source', value=int(legal))
        self.emit('assert_card', ref='source', property='attacking', value=int(legal))
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        if legal:
            self.source_tapped = True
            self.life[self.other] -= self.power
        self.receipt()

    def blink(self):
        self.emit('choose_target', ref='source', player=self.owner)
        self.emit('cast', ref='support', value=1)
        self.emit('require_card', ref='support', property='in_hand', value=0)
        self.spend(self.owner, self.support_fee)
        self.zones['hand'][self.owner].remove(self.support)
        self.receipt(1, pending=(self.owner, self.support))
        self.emit('resolve')
        self.emit('refresh')
        self.zones['graveyard'][self.owner].append(self.support)
        self.emit('bind_permanent', ref='source', player=self.owner, value=self.csvid)
        self.receipt()

    def donate(self):
        self.emit('choose_player', player=self.owner, value=self.other)
        self.emit('choose_target', ref='source', player=self.owner)
        self.emit('activate', ref='support', value=1)
        self.trader_tapped = True
        self.receipt(1)
        self.emit('resolve')
        self.emit('refresh')
        self.move(self.name, self.owner, 'battlefield', 'battlefield', self.other)
        self.source_controller = self.other
        self.emit('bind_permanent', ref='source', player=self.other, value=self.csvid)
        self.receipt()

    def end(self, should_return, *, advance=False):
        # These existing helpers invoke production end_turn_phase, including
        # TRIGGER_EOT. They do not model an independently interruptible step.
        self.emit('advance_turn' if advance else 'cleanup')
        self.emit('refresh')
        if should_return:
            self.move(self.name, self.source_controller, 'battlefield', 'hand', self.owner)
            self.source_alive = False
        self.receipt()

    def recast(self):
        self.end(True, advance=True)
        self.end(False, advance=True)
        self.emit('bind_hand', ref='source', player=self.owner, value=self.csvid)
        dash = self.mode == 'recast_dash'
        fee = self.alternate if dash else self.regular
        self.mana[self.owner] = list(fee)
        for color, amount in enumerate(fee):
            self.emit('set_mana', player=self.owner, color=color, value=amount)
        self.emit('refresh')
        if not self.cast_source(dash):
            raise ValueError('recast fixture must pay a legal cost')
        self.resolve_source()
        self.end(dash)

    def case(self):
        from recipes import step

        self.receipt()
        if self.cast_source(self.use_dash, cancel=self.mode == 'cancel'):
            if self.mode == 'pending_replay':
                self.snapshot = len(self.steps)
            if self.mode == 'countered_dash':
                self.counter()
                self.end(False)
            else:
                self.resolve_source()
                if self.mode in ('dash_combat', 'normal_fresh_attack'):
                    self.combat(self.use_dash)
                if self.mode == 'destroyed':
                    self.emit('destroy', ref='source')
                    self.emit('refresh')
                    self.move(self.name, self.owner, 'battlefield', 'graveyard')
                    self.source_alive = False
                    self.receipt()
                if self.mode == 'blink':
                    self.blink()
                    self.combat(False)
                if self.mode == 'donated':
                    self.donate()
                if self.mode == 'return_replay':
                    self.snapshot = len(self.steps)
                if self.mode in ('recast_dash', 'recast_normal'):
                    self.recast()
                else:
                    self.end(self.use_dash and self.mode not in ('destroyed', 'blink'))
        suffix = self.steps[self.snapshot:]
        sequence = self.before + self.steps[:self.snapshot] + [step('capture')] + suffix
        sequence += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'DashCreatureRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Bounded end fixture runs the actual EOT trigger; no separately interruptible end-step stack boundary.',
                            'All creature spells start in the casting player’s main phase. The low-level cast driver does not observe front-end timing denial.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    return [DashCase(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]
