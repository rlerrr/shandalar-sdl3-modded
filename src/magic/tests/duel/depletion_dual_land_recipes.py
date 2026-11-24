"""Depletion dual lands through ordinary play, real mana and actual turns."""

import copy
import json
import re
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from hand_reveal_dual_land_recipes import SPEND


FAMILY = 'depletion_dual_land'
PROFILE = Path(__file__).with_name('data') / 'depletion_dual_land_wave8_profiles.json'
HANDLER = 'manalinkeh_card_land_cap'
UNTAP = "This land doesn't untap during your untap step if it has a depletion counter on it."
UPKEEP = 'At the beginning of your upkeep, remove a depletion counter from this land.'
MANA = re.compile(r'\{T\}: Add \{([WUBRG])\} or \{([WUBRG])\}\. Put a depletion counter on this land\.')
WITNESSES = ('Walking Atlas', 'Twiddle', 'Boomerang', *SPEND.values())
MODES = ('ordinary_entry', 'one_land_allowance', 'spend_first', 'spend_second', 'tapped_repeat_denied',
         'zero_depletion_untaps', 'single_upkeep_cycle', 'two_upkeep_cycles', 'manual_untap', 'split_controller_upkeeps',
         'opposing_turn_atlas', 'offturn_mana', 'reentry_reset', 'upkeep_ready_snapshot', 'atlas_pending_snapshot')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line') != 'Land' or record.get('mana_cost') != '' or record.get('keywords') != []):
        raise ValueError('whole ordinary standalone depletion land required')
    lines = record['oracle_text'].splitlines()
    output = MANA.fullmatch(lines[2]) if len(lines) == 3 and lines[:2] == [UNTAP, UPKEEP] else None
    if not output or output[1] == output[2]:
        raise ValueError('complete depletion untap, own upkeep and distinct mana clauses required')
    return dict(outputs=list(output.groups()), add_depletion=1, remove_in_own_upkeep=1)


def variants(params):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES]


def check_variants(params, selected):
    if (type(selected) is not list or not selected or any(type(variant) is not str for variant in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(params))):
        raise ValueError('distinct reviewed depletion variants required')


def spec(declaration, record):
    expected = classify(record)
    pin = profiles()['sources'].get(record['name'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not pin or pin['oracle'] != record
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(expected, sort_keys=True)):
        raise ValueError('whole pinned Oracle and exact typed depletion parameters required')
    check_variants(expected, declaration.get('variants'))
    return dict(expected, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    from recipes import COLORS, cost_from_oracle

    data = profiles()
    catalog = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))
    unimplemented = {row['csvid'] for row in catalog['cases'].values()}
    if data.get('version') != 1 or set(data['witnesses']) != set(WITNESSES):
        raise ValueError('reviewed depletion witness matrix changed')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            binding = registry.get(name, {})
            if (oracle.get(name) != pin['oracle'] or binding != pin['registry'] or binding.get('kind') != 'card'
                    or binding['csvid'] in unimplemented or binding['handler'] == 'NULL'
                    or binding['handler'] == 'card_dummy' and not pin.get('reviewed_native_characteristics')):
                raise ValueError(f'whole Oracle or binding changed for {name}')
            if group == 'sources' and binding['handler'] != HANDLER:
                raise ValueError(f'depletion source handler changed for {name}')
    for color, name in SPEND.items():
        cost = cost_from_oracle(oracle[name])
        if cost[COLORS[color]] != 1 or sum(cost) != 1 or not oracle[name]['type_line'].startswith('Creature — '):
            raise ValueError(f'single colored mana spending witness changed for {color}')


class DepletionWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, registry, oracle):
        super().__init__(name, params, owner, registry, oracle)
        self.lands = {}
        self.played_land = False

    def land(self, ref, player):
        self.add(self.name, ref, player, 'hand')
        self.lands[ref] = dict(player=player, zone='hand', tapped=False, depletion=0)

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()
        for ref, land in self.lands.items():
            active = land['zone'] == 'battlefield'
            self.observe(ref, 'in_play' if active else 'in_hand', 1)
            if active:
                self.observe(ref, 'tapped', land['tapped'])
                self.emit('assert_counter', ref=ref, counter_kind='depletion', value=land['depletion'])

    def enter(self, ref):
        land = self.lands[ref]
        player = land['player']
        copies = self.zones['battlefield'][player].count(self.name)
        self.zones['hand'][player].remove(self.name)
        self.zones['battlefield'][player].append(self.name)
        land.update(zone='battlefield', tapped=False, depletion=0)
        self.emit('bind_permanent_copy', ref=ref, player=player, value=self.rows[self.name]['csvid'], index=copies)
        self.receipts()

    def play(self, ref='source'):
        land = self.lands[ref]
        allowed = land['zone'] == 'hand' and land['player'] == self.current and not self.played_land
        self.emit('play_land', ref=ref, value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        if allowed:
            self.played_land = True
            self.enter(ref)
        else:
            self.receipts()

    def atlas_place(self):
        self.emit('require_can_activate', ref='atlas', value=1)
        self.emit('activate', ref='atlas', value=1)
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 1)
        self.entities['atlas']['tapped'] = True
        self.observe('atlas', 'tapped', 1)
        cut = len(self.steps)
        self.emit('choose_zone_card', player=self.owner, value=self.rows[self.name]['csvid'])
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.enter('source')
        return cut

    def produce(self, ref, color):
        from recipes import COLORS

        state = self.lands[ref]
        if state['zone'] != 'battlefield' or state['tapped'] or color not in self.p['outputs']:
            raise ValueError('production requires an untapped land and a printed color')
        player = state['player']
        self.emit('require_can_activate', ref=ref, value=1)
        self.emit('choose_color', player=player, value=COLORS[color])
        self.emit('activate', ref=ref, value=1)
        self.emit('assert_unused_choices', value=0)
        state['tapped'] = True
        state['depletion'] += self.p['add_depletion']
        self.mana[player][COLORS[color]] += 1
        self.receipts()

    def deny_tapped(self, ref='source'):
        self.emit('require_can_activate', ref=ref, value=0)
        self.emit('activate', ref=ref, value=0)
        self.emit('assert_unused_choices', value=0)
        self.receipts()

    def spend(self, color, ref):
        from recipes import cost_from_oracle

        player = self.entities[ref]['player']
        cost = cost_from_oracle(self.o[SPEND[color]])
        if self.mana[player] != cost or self.current != player:
            raise ValueError('spending cast must use the land’s actual single output in its own main phase')
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.mana[player] = [0] * 6
        self.arrive(ref)
        if self.has(ref, 'Haste'):
            self.entities[ref]['sick'] = False
        self.receipts()

    def twiddle(self, target, *, untap):
        player = 1 - self.owner
        self.spell('Twiddle', 'twiddle', player, target, dialog=int(untap))
        self.lands[target]['tapped'] = not untap
        self.receipts()

    def bounce(self):
        self.spell('Boomerang', 'bounce', 1 - self.owner, 'source')
        self.zones['battlefield'][self.owner].remove(self.name)
        self.zones['hand'][self.owner].append(self.name)
        self.lands['source'].update(zone='hand', tapped=False, depletion=0)
        self.emit('bind_hand_copy', ref='source', player=self.owner, value=self.rows[self.name]['csvid'], index=0)
        self.receipts()

    def advance_to_untap(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('authored turn transition requires spent pools')
        self.emit('advance_turn')
        self.expire()
        self.current = 1 - self.current
        self.played_land = False
        self.emit('untap_step', player=self.current)
        for creature in self.entities.values():
            if creature['zone'] == 'battlefield' and creature['player'] == self.current:
                creature['tapped'] = creature['sick'] = False
        for land in self.lands.values():
            if land['zone'] == 'battlefield' and land['player'] == self.current and not land['depletion']:
                land['tapped'] = False
        self.receipts()

    def upkeep_and_main(self):
        self.emit('upkeep_step', player=self.current)
        for land in self.lands.values():
            if land['zone'] == 'battlefield' and land['player'] == self.current and land['depletion']:
                land['depletion'] -= self.p['remove_in_own_upkeep']
        self.receipts()
        self.emit('turn', player=self.current, value=0)

    def real_turn(self):
        self.advance_to_untap()
        self.upkeep_and_main()


def scenario(world, mode):
    owner, enemy = world.owner, 1 - world.owner
    first, second = world.p['outputs']
    world.land('source', owner)
    if mode in ('one_land_allowance', 'split_controller_upkeeps'):
        world.land('extra', owner if mode == 'one_land_allowance' else enemy)
    if mode in ('opposing_turn_atlas', 'reentry_reset', 'atlas_pending_snapshot'):
        world.add('Walking Atlas', 'atlas', owner)
    if mode in ('zero_depletion_untaps', 'manual_untap', 'two_upkeep_cycles'):
        world.add('Twiddle', 'twiddle', enemy, 'hand')
    if mode == 'reentry_reset':
        world.add('Boomerang', 'bounce', enemy, 'hand')
    first_spend = ('spend_first', 'tapped_repeat_denied', 'manual_untap', 'single_upkeep_cycle', 'two_upkeep_cycles',
                   'split_controller_upkeeps', 'upkeep_ready_snapshot', 'reentry_reset')
    if mode in first_spend or mode == 'spend_second':
        world.add(SPEND[second if mode == 'spend_second' else first], 'spend', owner, 'hand')
    if mode in ('single_upkeep_cycle', 'two_upkeep_cycles', 'split_controller_upkeeps'):
        world.add(SPEND[second], 'spend_next', enemy if mode == 'split_controller_upkeeps' else owner, 'hand')

    world.emit('turn', player=owner, value=0)
    world.emit('present_ai_dialogs', value=1)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.receipts()

    if mode == 'atlas_pending_snapshot':
        return world.atlas_place()
    if mode == 'opposing_turn_atlas':
        world.real_turn()
        world.play()
        world.atlas_place()
        world.produce('source', second)
        return None

    world.play()
    if mode == 'one_land_allowance':
        world.play('extra')
        world.real_turn()
        world.real_turn()
        world.play('extra')
    elif mode == 'ordinary_entry':
        world.emit('require_can_activate', ref='source', value=1)
    elif mode == 'zero_depletion_untaps':
        world.twiddle('source', untap=False)
        world.real_turn()
        world.real_turn()
        world.emit('require_can_activate', ref='source', value=1)
    elif mode == 'offturn_mana':
        world.real_turn()
        world.produce('source', second)
    else:
        chosen = second if mode == 'spend_second' else first
        world.produce('source', chosen)
        world.spend(chosen, 'spend')

    if mode == 'tapped_repeat_denied':
        world.deny_tapped()
    if mode in ('manual_untap', 'two_upkeep_cycles'):
        world.twiddle('source', untap=True)
        world.emit('require_can_activate', ref='source', value=1)
        if mode == 'two_upkeep_cycles':
            world.produce('source', second)
            world.spend(second, 'spend_next')
            for _ in range(2):
                world.real_turn()
                world.real_turn()
                world.deny_tapped()
            world.real_turn()
            world.real_turn()
            world.emit('require_can_activate', ref='source', value=1)
    if mode in ('single_upkeep_cycle', 'upkeep_ready_snapshot'):
        world.real_turn()
        world.advance_to_untap()
        cut = len(world.steps) if mode == 'upkeep_ready_snapshot' else None
        world.upkeep_and_main()
        world.deny_tapped()
        world.real_turn()
        world.real_turn()
        if mode == 'single_upkeep_cycle':
            world.produce('source', second)
            world.spend(second, 'spend_next')
        return cut
    if mode == 'split_controller_upkeeps':
        world.real_turn()
        world.play('extra')
        world.produce('extra', second)
        world.spend(second, 'spend_next')
        world.real_turn()
        world.real_turn()
        world.real_turn()
    if mode == 'reentry_reset':
        world.bounce()
        world.play()
        world.atlas_place()
        world.emit('require_can_activate', ref='source', value=1)
    return None


def expand(name, csvid, declaration, params, selected, registry, oracle):
    from recipes import step

    check_variants(params, selected)
    validate_witnesses(oracle, registry)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('depletion source CSVID changed')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = DepletionWorld(name, params, int(owner), registry, oracle)
        cut = scenario(world, mode)
        actions = world.steps
        if cut is not None:
            tail = actions[cut:]
            actions = (actions[:cut] + [step('capture')] + tail + [step('record_state'), step('ai_roundtrip'), step('restore')]
                       + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'DepletionDualLandRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=actions, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'],
                              untested_components=['production priority menu and extra land-play effects',
                                                   'proliferation, external counter replacement, and multiplayer'])))
    return cases
