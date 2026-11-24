"""Oracle-derived combat-wide prevention and its coupled costs and effects."""

import copy
import json
import re
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'combat_wide_fog'
FOG = 'Prevent all combat damage that would be dealt this turn.'
BUYBACK = ('Buyback—Sacrifice a land. (You may sacrifice a land in addition to any other costs as you cast this spell. '
           'If you do, put this card into your hand as it resolves.)')
CLASH = (" Clash with an opponent. If you win, creatures that player controls don't untap during the player's next untap step. "
         '(Each clashing player reveals the top card of their library, then puts that card on their choice of the top or bottom. '
         'A player wins if their card had a greater mana value.)')
COLOR_FOG = ('Prevent all combat damage that would be dealt this turn if {W} was spent to cast this spell. '
             'Each player loses 1 life for each attacking creature they control if {B} was spent to cast this spell. '
             '(Do both if {W}{B} was spent.)')
SPIDER = ('Create X 1/2 green Spider creature tokens with reach, where X is the number of creatures attacking you. '
          'Prevent all combat damage that would be dealt this turn by non-Spider creatures.')


def profiles():
    path = Path(__file__).parent / 'data/combat_wide_fog_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or record['type_line'].startswith('Token ')):
        raise ValueError('whole normal source required')
    text = record['oracle_text']
    kind = record['type_line']
    activation = re.fullmatch(r'((?:\{[0-9WUBRG]+\})+), \{T\}, Discard a card: ' + re.escape(FOG), text)
    ability = [0] * 6
    if activation and kind.startswith('Creature — ') and record['power'].isdigit() and record['toughness'].isdigit():
        model = 'tap_discard'
        ability = cost_from_oracle(dict(record, mana_cost=activation[1]))
    elif kind == 'Instant' and text == BUYBACK + '\n' + FOG:
        model = 'land_buyback'
    elif kind == 'Instant' and text == FOG + CLASH:
        model = 'clash_untap'
    elif kind == 'Instant' and text == COLOR_FOG and record['mana_cost'] == '{1}{W/B}':
        model = 'color_fog_loss'
    elif kind == 'Instant' and text == SPIDER:
        model = 'spider_fog'
    else:
        raise ValueError('complete reviewed combat-wide prevention clauses required')
    costs = cost_from_oracle(record) if model != 'color_fog_loss' else None
    return dict(model=model, cast_mana=costs, activation_mana=ability)


def spec(declaration, record):
    if (not record or declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or 'variants' not in declaration):
        raise ValueError('typed whole Oracle declaration required')
    expected = classify(record)
    params = declaration.get('params')
    if params != expected or type(params) is not dict:
        raise ValueError('exact Oracle model required')
    for key in ('cast_mana', 'activation_mana'):
        if params[key] is not None and (type(params[key]) is not list or any(type(n) is not int for n in params[key])):
            raise ValueError('whole payment vectors require integer units')
    pin = profiles()['sources'].get(record['name'])
    if not pin or pin['oracle'] != record:
        raise ValueError('independent released source pin required')
    return dict(copy.deepcopy(params), family=FAMILY, effect=FAMILY, keywords=[])


def validate_witnesses(oracle, registry):
    data = profiles()
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = registry.get(name)
            if not row or row['handler'] != pin['handler'] or row['kind'] != pin['kind'] or oracle.get(name) != pin['oracle']:
                raise ValueError('independent Oracle/handler pin changed: ' + name)
            if row['handler'] == 'NULL' or (row['handler'] == 'card_dummy' and not pin.get('reviewed_native_witness')):
                raise ValueError('unsupported implementation: ' + name)
            if group == 'sources' and (row['kind'] != 'card' or oracle[name]['layout'] != 'normal' or row['handler'] == 'card_dummy'):
                raise ValueError('normal implemented primary required: ' + name)


def variants(params):
    model = params['model']
    modes = ['defend', 'own_attack', 'no_effect', 'noncombat', 'insufficient', 'wrong_color', 'pending_snapshot', 'zero_attackers']
    if model != 'clash_untap':
        modes.append('after_turn')
    modes += {
        'land_buyback': ['buyback', 'pass_buyback', 'foreign_land_only'],
        'tap_discard': ['cast_permanent', 'fresh_source', 'tapped_source', 'no_discard', 'source_lost_pending'],
        'color_fog_loss': ['black', 'both', 'black_own_attack', 'both_own_attack', 'black_zero', 'both_zero'],
        'clash_untap': ['clash_win', 'clash_lose', 'clash_tie', 'human_bottom'],
        'spider_fog': ['spider_exempt', 'reach_block', 'spider_block'],
    }[model]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


class FogWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.fog = None
        self.spiders = []
        self.land_refs = {}
        self.locked = set()
        self.discard = None

    def add(self, name, ref, player, zone='battlefield', *, source=False, tapped=False):
        super().add(name, ref, player, zone, source)
        if tapped:
            self.setup[-1]['state'] = ['tapped']
        if ref in self.entities:
            self.entities[ref]['tapped'] = tapped
        if zone == 'battlefield' and 'Land' in self.o[name]['type_line']:
            self.land_refs[ref] = (name, player, tapped)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        super().receipts()
        for ref, (_, _, tapped) in self.land_refs.items():
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', tapped)
        if self.p['model'] == 'spider_fog':
            spider_id = self.rows['Spider']['csvid']
            for player in (0, 1):
                count = self.zones['battlefield'][player].count('Spider')
                self.emit('assert_zone_multiplicity', player=player, zone='battlefield', index=spider_id, value=count)
                if count:
                    for prop, value in (('token', 1), ('color', 8), ('subtype_Spider', 1), ('power', 1), ('toughness', 2)):
                        self.emit('assert_matching_permanents', player=player, index=spider_id, property=prop, value=value)

    def main(self, player):
        self.current = player
        self.emit('turn', player=player, value=0)

    def pay(self, player, price):
        balance = [available - due for available, due in zip(self.mana[player], price)]
        if min(balance) < 0:
            raise ValueError('nonpayable fixture action')
        self.mana[player] = balance

    def cast_permanent(self):
        price = self.p['cast_mana']
        self.pool(self.owner, price)
        self.emit('refresh')
        self.emit('cast', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.pay(self.owner, price)
        self.emit('resolve')
        self.arrive('source')
        self.receipts()

    def declare(self, attackers):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref]['tapped'] = self.entities[ref]['attacking'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')

    def assign_blocker(self, attacker, blocker):
        self.emit('block', ref=blocker, other=attacker, value=1)
        self.observe(blocker, 'blocking_count', 1)

    def is_spider(self, ref):
        return 'Spider' in self.o[self.entities[ref]['name']]['type_line'].split(' — ')[-1].split()

    def combat_amount(self, ref):
        if self.fog == 'all' or self.fog == 'non_spider' and not self.is_spider(ref):
            return 0
        return self.entities[ref]['power']

    def combat_damage(self, attackers, pair=None):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for attacker in attackers:
            amount = self.combat_amount(attacker)
            if pair and attacker == pair[0]:
                blocker = pair[1]
                self.entities[blocker]['damage'] += amount
                self.entities[attacker]['damage'] += self.combat_amount(blocker)
            else:
                self.life[1-self.current] -= amount
        for ref, entity in self.entities.items():
            entity['attacking'] = False
            if entity['zone'] == 'battlefield' and entity['damage'] >= entity['toughness']:
                self.leave(ref, 'graveyard')
        self.receipts()

    def spawn_spiders(self, count):
        for index in range(count):
            ref = f'spider_{index}'
            self.entity('Spider', ref, self.owner, 'battlefield', token=True)
            # The pinned source specifies its token's stats. Other cards can
            # create different Spider tokens through the same registry ID.
            self.entities[ref].update(power=1, toughness=2)
            self.zones['battlefield'][self.owner].append('Spider')
            self.emit('bind_permanent_copy', player=self.owner, value=self.rows['Spider']['csvid'], index=index, ref=ref)
            self.spiders.append(ref)

    def pass_turn(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('real untap requires naturally spent pools')
        self.emit('advance_turn')
        self.current = 1 - self.current
        self.fog = None
        for entity in self.entities.values():
            entity['attacking'] = False
            entity['damage'] = 0
        self.emit('untap_step', player=self.current)
        for ref, entity in self.entities.items():
            if entity['zone'] == 'battlefield' and entity['player'] == self.current:
                if ref not in self.locked:
                    entity['tapped'] = False
                entity['sick'] = False
                self.locked.discard(ref)
        for ref, (name, player, tapped) in self.land_refs.items():
            if player == self.current:
                self.land_refs[ref] = (name, player, False)
        self.receipts()
        self.main(self.current)

    def direct_damage(self):
        by = self.current
        fee = [0, 0, 0, 0, 1, 0]
        self.pool(by, fee)
        self.emit('choose_player', player=by, value=1-by)
        self.emit('cast', ref='bolt', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.pay(by, fee)
        self.zones['hand'][by].remove('Lightning Bolt')
        self.zones['graveyard'][by].append('Lightning Bolt')
        self.life[1-by] -= 3
        self.receipts()


def _payment(params, mode):
    model = params['model']
    if model == 'tap_discard':
        price = list(params['activation_mana'])
    elif model == 'color_fog_loss':
        if mode.startswith('black'):
            price = [1, 1, 0, 0, 0, 0]
        elif mode.startswith('both'):
            price = [0, 1, 0, 0, 0, 1]
        else:
            price = [1, 0, 0, 0, 0, 1]
    else:
        price = list(params['cast_mana'])
    if mode == 'insufficient':
        return [0] * 6
    if mode == 'wrong_color':
        return [sum(price), 0, 0, 0, 0, 0]
    return price


def _queue_clash(world, mode):
    owner = world.owner
    # The two existing informational reveals precede the first top/bottom
    # selection. The human may move that player's actual shown card to bottom.
    world.emit('choose_dialog', player=owner, value=0)
    world.emit('choose_dialog', player=owner, value=0)
    if mode == 'human_bottom':
        world.emit('choose_zone_card', player=0, value=world.rows[world.zones['library'][0][0]]['csvid'])
    else:
        world.emit('choose_zone', player=0, value=-1)


def _finish_clash(world, mode):
    owner = world.owner
    ours = world.o[world.zones['library'][owner][0]]
    theirs = world.o[world.zones['library'][1-owner][0]]
    if float(ours['cmc']) > float(theirs['cmc']):
        world.locked = {ref for ref, card in world.entities.items()
                        if card['zone'] == 'battlefield' and card['player'] == 1-owner}
    if mode == 'human_bottom':
        library = world.zones['library'][0]
        library.append(library.pop(0))


def _apply_source(world, mode, attackers, price):
    owner, model = world.owner, world.p['model']
    buys_back = mode == 'buyback'
    if model == 'land_buyback' and 'buyback_land' in world.land_refs:
        world.emit('choose_dialog', player=owner, value=0 if buys_back else 1)
        if buys_back and owner == 0:
            world.emit('choose_target', player=owner, ref='buyback_land')
    if model == 'color_fog_loss':
        menu = 2 if mode.startswith('both') else 1 if mode.startswith('black') else 0
        world.emit('choose_dialog', player=owner, value=menu)
    op = 'activate' if model == 'tap_discard' else 'cast'
    world.emit(op, ref='source', value=1)
    world.emit('assert_unused_choices', value=0)
    world.emit('assert_stack', value=1)
    world.pay(owner, price)
    if buys_back:
        name, player, _ = world.land_refs.pop('buyback_land')
        world.zones['battlefield'][player].remove(name)
        world.zones['graveyard'][player].append(name)
        world.emit('assert_zone_multiplicity', player=owner, zone='graveyard', index=world.rows[name]['csvid'], value=1)
    if model == 'tap_discard':
        world.zones['hand'][owner].remove(world.discard)
        world.zones['graveyard'][owner].append(world.discard)
        world.entities['source']['tapped'] = True
        world.observe('source', 'tapped', 1)
        world.emit('assert_zone_multiplicity', player=owner, zone='graveyard',
                   index=world.rows[world.discard]['csvid'], value=1)
    cut = len(world.steps)
    if mode == 'source_lost_pending':
        world.emit('destroy', ref='source')
        world.leave('source', 'graveyard')
    if model == 'clash_untap':
        _queue_clash(world, mode)
    world.emit('resolve')
    world.emit('assert_unused_choices', value=0)
    if model != 'tap_discard' and not buys_back:
        world.zones['hand'][owner].remove(world.name)
        world.zones['graveyard'][owner].append(world.name)
    if model == 'color_fog_loss':
        if price[1]:
            world.life[world.current] -= len(attackers)
        if price[5]:
            world.fog = 'all'
    elif model == 'spider_fog':
        world.fog = 'non_spider'
        world.spawn_spiders(len(attackers) if world.current != owner else 0)
    else:
        world.fog = 'all'
        if model == 'clash_untap':
            _finish_clash(world, mode)
    world.receipts()
    return cut


def _scenario(world, mode):
    owner, model = world.owner, world.p['model']
    own_attack = mode in ('own_attack', 'black_own_attack', 'both_own_attack', 'fresh_source')
    actor = owner if own_attack else 1 - owner
    is_permanent = model == 'tap_discard'
    cast_only = mode == 'cast_permanent'
    source_zone = 'hand' if not is_permanent or cast_only or mode == 'fresh_source' else 'battlefield'
    world.add(world.name, 'source', owner, source_zone, source=True, tapped=mode == 'tapped_source')
    attackers = () if mode in ('zero_attackers', 'black_zero', 'both_zero') else ('large', 'other')
    selected = ('Air Elemental' if mode == 'reach_block' else 'Giant Spider'
                if mode in ('spider_block', 'spider_exempt') else 'Craw Wurm')
    if not cast_only:
        world.add(selected, 'large', actor)
        world.add('Hill Giant', 'other', actor)
        world.add('Grizzly Bears', 'blocker', 1-actor)
    if model == 'land_buyback' and mode in ('buyback', 'pass_buyback', 'foreign_land_only'):
        land_owner = 1-owner if mode == 'foreign_land_only' else owner
        ref = 'foreign_land' if mode == 'foreign_land_only' else 'buyback_land'
        world.add('Forest', ref, land_owner, tapped=True)
    if is_permanent and mode not in ('no_discard', 'cast_permanent'):
        world.discard = 'Plains'
        world.add(world.discard, 'discard', owner, 'hand')
    if mode == 'noncombat':
        world.add('Lightning Bolt', 'bolt', actor, 'hand')

    library_tops = ['Island', 'Island']
    if model == 'clash_untap':
        if mode == 'clash_lose':
            library_tops[1-owner] = 'Craw Wurm'
        elif mode != 'clash_tie':
            library_tops[owner] = 'Craw Wurm'
        for i in range(4):
            world.add('Swamp', f'quiet_ai_land_{i}', 1, tapped=True)
        for player in (0, 1):
            world.add('Grizzly Bears', f'idle_{player}', player, tapped=True)
    for player in (0, 1):
        for index, name in enumerate((library_tops[player], 'Forest', 'Plains')):
            world.add(name, f'library_{player}_{index}', player, 'library')
        world.add('Swamp', f'grave_{player}', player, 'graveyard')
        world.add('Mountain', f'exile_{player}', player, 'exile')

    world.main(owner if cast_only else actor)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('discard_policy', value=1)
    if cast_only or mode == 'fresh_source':
        world.cast_permanent()
        if cast_only:
            return None
    price = _payment(world.p, mode)
    world.pool(owner, price if mode != 'no_effect' else [0] * 6)
    world.emit('refresh')
    world.receipts()
    world.declare(attackers)
    use_spider = mode in ('reach_block', 'spider_block')
    pair = None
    if attackers and not use_spider and mode != 'spider_exempt':
        pair = ('large', 'blocker')
        world.assign_blocker(*pair)
    if not use_spider:
        world.emit('advance_combat')

    denied = mode in ('insufficient', 'wrong_color', 'no_discard', 'tapped_source', 'fresh_source')
    cut = None
    if denied:
        world.emit('activate' if is_permanent else 'cast', ref='source', value=0)
        world.emit('assert_unused_choices', value=0)
        world.receipts()
    elif mode != 'no_effect':
        cut = _apply_source(world, mode, attackers, price)
        if use_spider:
            pair = ('large', world.spiders[0])
            world.assign_blocker(*pair)
            world.emit('advance_combat')
        if mode == 'noncombat':
            world.direct_damage()

    world.combat_damage(attackers, pair)
    if mode == 'after_turn':
        world.pass_turn()
        world.pass_turn()
        world.declare(attackers)
        world.emit('advance_combat')
        world.combat_damage(attackers)
    if model == 'clash_untap' and mode in ('clash_win', 'clash_lose', 'clash_tie', 'human_bottom'):
        # Visit two full rotations so the opposing creature can prove both a
        # skipped next untap and normal untap after the restriction expires.
        for _ in range(4 if mode in ('clash_win', 'human_bottom') else 2):
            world.pass_turn()
    return cut


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    if not selected or len(set(selected)) != len(selected) or any(value not in variants(params) for value in selected):
        raise ValueError('explicit distinct Fog variants required')
    validate_witnesses(oracle, rows)
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        world = FogWorld(name, params, int(owner), rows, oracle)
        cut = _scenario(world, mode)
        steps = world.steps
        if mode == 'pending_snapshot':
            assert cut is not None
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + copy.deepcopy(tail)
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        cases.append(dict(name=f'CombatWideFogRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                          setup=world.setup, steps=steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'],
                              untested_components=['planeswalker recipients and multiplayer',
                                                   'damage that cannot be prevented and multiple blockers',
                                                   'unconstrained AI choice strategy and repeated buyback casts'])))
    return cases
