"""Selected-creature prevention, untap and toughness through production actions."""

import copy
import json
import re
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'targeted_damage_defense'
PUMP = 'That creature gets +0/+X until end of turn, where X is its mana value.'
MANA_ABILITY = '{T}: Add {C}.'
UNTAP = re.compile(r'Untap target (attacking )?creature( you control)?\. ')
DIRECT = re.compile(r'Prevent all (combat )?damage that would be dealt (to and dealt )?by target (attacking )?creature( you control)? this turn\.')
INVERTED = re.compile(r'Prevent all damage that would be dealt this turn by target (attacking )?creature( you control)?\.')
LINKED = re.compile(r'Prevent all (combat )?damage that would be dealt (to and dealt )?by (?:that creature|it) this turn\.')


def _profiles():
    path = Path(__file__).parent / 'data/targeted_damage_defense_wave8_profiles.json'
    return json.loads(path.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal source required')
    if record.get('keywords'):
        raise ValueError('independent printed source keywords require a separate model')
    type_line = record['type_line']
    kinds = [kind for kind in ('Creature', 'Artifact', 'Land', 'Instant') if kind in type_line.split(' — ')[0].split()]
    if len(kinds) != 1:
        raise ValueError('standalone creature, artifact, land or instant required')
    kind = kinds[0]
    lines = record['oracle_text'].splitlines()
    mana_ability = len(lines) == 2 and lines[0] == MANA_ABILITY and kind == 'Land'
    if len(lines) != 1 + mana_ability:
        raise ValueError('additional whole-card clauses need their own receipt model')
    text = lines[-1]
    fee = [0] * 6
    if kind != 'Instant':
        activation = re.fullmatch(r'((?:\{[0-9WUBRG]+\})+(?:, ))?\{T\}: (.+)', text)
        if not activation:
            raise ValueError('one printed tap activation with optional fixed mana required')
        if activation[1]:
            fee = cost_from_oracle(dict(record, mana_cost=activation[1][:-2]))
        text = activation[2]
    untap = UNTAP.match(text)
    pump = text.endswith(' ' + PUMP)
    if pump:
        text = text[:-len(PUMP)-1]
    if untap:
        linked = LINKED.fullmatch(text[untap.end():])
        if not linked:
            raise ValueError('untap and prevention must refer to the same target')
        combat_only, incoming = map(bool, linked.groups())
        attacking, own = map(bool, untap.groups())
    elif direct := DIRECT.fullmatch(text):
        combat_only, incoming, attacking, own = map(bool, direct.groups())
    elif inverted := INVERTED.fullmatch(text):
        combat_only = incoming = False
        attacking, own = map(bool, inverted.groups())
    else:
        raise ValueError('whole selected-creature damage prevention required')
    if kind == 'Creature' and not (record.get('power', '').isdigit() and record.get('toughness', '').isdigit()):
        raise ValueError('fixed creature stats required')
    if kind != 'Land':
        cost_from_oracle(record)
    return dict(kind=kind.lower(), activation_mana=fee, mana_ability=mana_ability,
                combat_only=combat_only, incoming=incoming, attacking_target=attacking,
                own_target=own, untap=bool(untap), toughness_by_mana_value=pump)


def spec(declaration, record):
    from recipes import cost_from_oracle

    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not record
            or declaration.get('oracle_text') != record['oracle_text'] or 'variants' not in declaration):
        raise ValueError('typed version-two complete Oracle declaration required')
    params = declaration.get('params')
    expected = classify(record)
    if (type(params) is not dict or params != expected
            or any(type(params[key]) is not type(value) for key, value in expected.items())
            or any(type(value) is not int for value in params['activation_mana'])):
        raise ValueError('exact typed Oracle-derived prevention parameters required')
    pin = _profiles()['sources'].get(record['name'])
    if not pin or pin['oracle'] != record:
        raise ValueError('independent released source pin required')
    if pin['input'] != ('kor_modal_menu_0_mana_1_prevention_2_cancel' if expected['mana_ability'] else 'single_target'):
        raise ValueError('unreviewed presentation contract')
    return dict(copy.deepcopy(params), family=FAMILY, effect=FAMILY, keywords=[],
                cast_mana=cost_from_oracle(record) if expected['kind'] != 'land' else [0] * 6)


def validate_witnesses(oracle, registry):
    profile = _profiles()
    for group in ('sources', 'witnesses'):
        for name, pin in profile[group].items():
            row = registry.get(name)
            if (not row or row['kind'] != 'card' or row['handler'] == 'NULL' or row['handler'] != pin['handler']
                    or oracle.get(name) != pin['oracle'] or oracle[name]['layout'] != 'normal'
                    or (row['handler'] == 'card_dummy' and (group == 'sources' or not pin.get('reviewed_native_witness')))):
                raise ValueError('full Oracle/handler pin mismatch: ' + name)


def variants(params):
    modes = ['unblocked', 'blocked', 'unprotected', 'noncombat_incoming', 'noncombat_outgoing',
             'no_eligible_target', 'target_lost_pending', 'pending_snapshot', 'resolved_snapshot',
             'cleanup', 'bare_zones']
    if not params['own_target']:
        modes.append('own_attacker')
    if not params['attacking_target']:
        modes.append('blocker_target')
    if params['kind'] in ('creature', 'artifact'):
        modes.extend(['cast', 'cast_insufficient', 'source_lost_pending'])
    if params['kind'] == 'creature':
        modes.extend(['cast_wrong_color', 'fresh_source', 'foreign_attacker'])
    if params['kind'] != 'instant':
        modes.append('tapped_source')
    if any(params['activation_mana']) and not params['mana_ability']:
        modes.append('insufficient_payment')
    if any(params['activation_mana'][1:]) and not params['mana_ability'] or params['kind'] == 'instant':
        modes.append('wrong_color')
    if params['mana_ability']:
        modes.extend(['mana', 'exact_payment', 'no_white_mana', 'mode_cancel'])
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


class DefenseWorld(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.source_zone = None
        self.source_tapped = False
        self.protected = None

    def add_source(self, zone, tapped):
        self.add(self.name, 'source', self.owner, zone, source=True)
        self.source_zone = zone
        self.source_tapped = tapped
        if tapped:
            self.setup[-1]['state'] = ['tapped']
            if 'source' in self.entities:
                self.entities['source']['tapped'] = True

    def observe(self, ref, property_name, value):
        self.emit('assert_card', ref=ref, property=property_name, value=int(value))

    def receipts(self):
        super().receipts()
        if self.source_zone == 'battlefield' and 'source' not in self.entities:
            self.observe('source', 'in_play', 1)
            self.observe('source', 'tapped', self.source_tapped)

    def pay(self, player, fee):
        after = [present - needed for present, needed in zip(self.mana[player], fee)]
        if min(after) < 0:
            raise ValueError('fixture payment must supply complete printed costs')
        self.mana[player] = after

    def move_source(self, zone):
        self.zones[self.source_zone][self.owner].remove(self.name)
        self.zones[zone][self.owner].append(self.name)
        self.source_zone = zone
        if 'source' in self.entities:
            self.entities['source']['zone'] = zone

    def cast_permanent(self, denied=None):
        fee = self.p['cast_mana']
        pool = [0] * 6 if denied == 'cast_insufficient' else list(fee)
        if denied == 'cast_wrong_color':
            color = next(i for i in range(1, 6) if pool[i])
            pool[0] += pool[color]
            pool[color] = 0
        self.pool(self.owner, pool)
        self.emit('refresh')
        self.emit('cast', ref='source', value=int(denied is None))
        if denied:
            return
        self.emit('assert_stack', value=1)
        self.observe('source', 'in_hand', 0)
        self.pay(self.owner, fee)
        self.emit('resolve')
        self.move_source('battlefield')
        if 'source' in self.entities:
            self.entities['source']['sick'] = True
        self.receipts()

    def set_actor(self, player):
        self.current = player
        self.emit('turn', player=player, value=0)

    def declare(self, attackers, blocked_target=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref]['attacking'] = self.entities[ref]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocked_target:
            self.emit('block', ref='blocker', other=blocked_target, value=1)
            self.observe('blocker', 'blocking_count', 1)
        self.emit('advance_combat')

    def apply_defense(self, ref):
        entity = self.entities[ref]
        self.protected = ref
        if self.p['untap']:
            entity['tapped'] = False
        if self.p['toughness_by_mana_value']:
            entity['toughness'] += self.oracle_mv(ref)

    def oracle_mv(self, ref):
        from recipes import cost_from_oracle

        return sum(cost_from_oracle(self.o[self.entities[ref]['name']]))

    def damage(self, source, amount, *, target=None, player=None, combat=False):
        if self.protected is not None:
            applies = combat or not self.p['combat_only']
            if applies and (source == self.protected or self.p['incoming'] and target == self.protected):
                amount = 0
        if player is not None:
            self.life[player] -= amount
        else:
            self.entities[target]['damage'] += amount

    def check_deaths(self):
        for ref, entity in self.entities.items():
            if entity['zone'] == 'battlefield' and entity['damage'] >= entity['toughness']:
                self.leave(ref, 'graveyard')

    def combat(self, attackers, blocked_target):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            entity = self.entities[ref]
            if entity['zone'] != 'battlefield':
                continue
            if ref == blocked_target:
                blocker = self.entities['blocker']
                self.damage(ref, entity['power'], target='blocker', combat=True)
                self.damage('blocker', blocker['power'], target=ref, combat=True)
            else:
                self.damage(ref, entity['power'], player=1-self.current, combat=True)
        self.check_deaths()
        for entity in self.entities.values():
            entity['attacking'] = False
        self.receipts()

    def activation(self, ref):
        is_spell = self.p['kind'] == 'instant'
        if self.p['mana_ability']:
            self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('choose_target', player=self.owner, ref=ref)
        self.emit('cast' if is_spell else 'activate', ref='source', value=1)
        # Report an unexpectedly unavailable Kor mode as ordinary receipts.
        # Drain immediately because on that path the native callback makes mana.
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.pay(self.owner, self.p['cast_mana'] if is_spell else self.p['activation_mana'])
        if not is_spell:
            self.source_tapped = True
            self.observe('source', 'tapped', 1)
            if 'source' in self.entities:
                self.entities['source']['tapped'] = True
        return len(self.steps)

    def finish_defense(self, target, mode):
        if mode == 'source_lost_pending':
            self.emit('destroy', ref='source')
            self.move_source('graveyard')
        if mode == 'target_lost_pending':
            self.emit('destroy', ref=target)
            self.leave(target, 'graveyard')
        self.emit('resolve')
        if self.p['kind'] == 'instant':
            self.move_source('graveyard')
        if mode != 'target_lost_pending':
            self.apply_defense(target)
        self.receipts()

    def witness_spell(self, name, ref, player, target=None):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.o[name])
        pool = self.mana[player]
        self.pool(player, [a+b for a, b in zip(pool, fee)])
        self.emit('refresh')
        if target:
            self.emit('choose_target', player=player, ref=target)
        self.emit('cast', ref=ref, value=1)
        self.emit('assert_stack', value=1)
        self.pay(player, fee)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.zones['hand'][player].remove(name)
        self.zones['graveyard'][player].append(name)

    def cleanup(self):
        self.emit('cleanup')
        self.emit('refresh')
        self.clear_turn_effects()
        self.receipts()

    def clear_turn_effects(self):
        self.protected = None
        for entity in self.entities.values():
            entity['toughness'] = int(self.o[entity['name']]['toughness'])
            entity['damage'] = 0

    def advance_and_untap(self):
        if any(any(pool) for pool in self.mana):
            raise ValueError('real untap requires both pools to be empty')
        self.emit('advance_turn')
        self.clear_turn_effects()
        self.current = 1 - self.current
        self.emit('untap_step', player=self.current)
        for entity in self.entities.values():
            if entity['player'] == self.current and entity['zone'] == 'battlefield':
                entity['tapped'] = entity['sick'] = False
        if self.current == self.owner and self.source_zone == 'battlefield':
            self.source_tapped = False
        self.receipts()


def _scenario(world, mode):
    p, owner = world.p, world.owner
    kind = p['kind']
    actor = owner if p['own_target'] or mode == 'own_attacker' else 1 - owner
    if mode == 'foreign_attacker' or mode == 'blocker_target' and p['own_target']:
        actor = 1 - owner
    cast_only = mode in ('cast', 'cast_insufficient', 'cast_wrong_color')
    no_target = mode == 'no_eligible_target'
    no_combat = cast_only or no_target or mode == 'mana'
    source_in_hand = kind == 'instant' or cast_only or mode == 'fresh_source'
    world.add_source('hand' if source_in_hand else 'battlefield', mode == 'tapped_source')

    selected_name = 'Prodigal Sorcerer' if mode == 'noncombat_outgoing' else 'Craw Wurm'
    if not no_combat:
        world.add(selected_name, 'selected', actor)
        world.add('Hill Giant', 'unselected', actor)
    elif no_target and p['attacking_target']:
        # A live creature exists, but no creature has been declared attacking.
        world.add('Craw Wurm', 'selected', actor)
    elif no_target and p['own_target']:
        # The only creature belongs to the other player.
        world.add('Craw Wurm', 'foreign', 1 - owner)

    blocked = mode in ('blocked', 'unprotected', 'blocker_target', 'cleanup')
    if blocked:
        name = 'Hill Giant' if mode == 'blocker_target' else 'Grizzly Bears'
        world.add(name, 'blocker', 1 - actor)
    if mode == 'noncombat_incoming':
        world.add('Lightning Bolt', 'bolt', 1 - actor, 'hand')
    real_expiry = mode == 'cleanup' and p['untap']
    need_vitalize = (mode == 'cleanup' or mode == 'noncombat_outgoing') and not p['untap']
    if need_vitalize:
        world.add('Vitalize', 'vitalize', actor, 'hand')
    if mode != 'bare_zones':
        for player in (0, 1):
            world.add('Island', f'library_{player}', player, 'library')
            world.add('Swamp', f'grave_{player}', player, 'graveyard')
            world.add('Mountain', f'exile_{player}', player, 'exile')

    # Permanent casts happen in their owner's main phase. All other setups
    # start directly in the chosen attacker's main phase.
    world.set_actor(owner if cast_only or mode == 'fresh_source' else actor)
    for player in (0, 1):
        world.pool(player, [0] * 6)
    world.emit('refresh')
    if cast_only or mode == 'fresh_source':
        world.cast_permanent(mode if mode in ('cast_insufficient', 'cast_wrong_color') else None)
        if cast_only:
            world.receipts()
            return 0, 0
        world.set_actor(actor)

    cast_fee = p['cast_mana'] if kind == 'instant' else p['activation_mana']
    payment = list(cast_fee)
    if p['mana_ability'] and mode not in ('exact_payment', 'mana', 'no_eligible_target', 'no_white_mana'):
        # The native modal test checks an extra generic mana before offering
        # the ability. Actual activation still deducts the printed fee only.
        payment[0] += 1
    if mode in ('insufficient_payment', 'mana'):
        payment = [0] * 6
    if mode in ('wrong_color', 'no_white_mana'):
        payment = [sum(cast_fee), 0, 0, 0, 0, 0]
    world.pool(owner, payment)
    world.emit('refresh')

    attackers = ('selected', 'unselected')
    blocked_target = 'selected' if blocked else None
    if not no_combat:
        world.declare(attackers, blocked_target)
    if p['mana_ability'] and mode in ('mana', 'no_eligible_target', 'no_white_mana'):
        world.emit('activate', ref='source', value=1)
        world.emit('assert_unused_choices', value=0)
        world.source_tapped = True
        world.mana[owner][0] += 1
        world.receipts()
    elif mode == 'mode_cancel':
        world.emit('choose_dialog', player=owner, value=2)
        world.emit('activate', ref='source', value=0)
        world.emit('assert_unused_choices', value=0)
        world.receipts()
    elif mode in ('no_eligible_target', 'tapped_source', 'fresh_source', 'foreign_attacker', 'insufficient_payment', 'wrong_color'):
        world.emit('cast' if kind == 'instant' else 'activate', ref='source', value=0)
        world.emit('assert_unused_choices', value=0)
        world.receipts()
    elif mode != 'unprotected':
        target = 'blocker' if mode == 'blocker_target' else 'selected'
        pending_cut = world.activation(target)
        world.finish_defense(target, mode)
        resolved_cut = len(world.steps)
        if mode == 'noncombat_incoming':
            world.witness_spell('Lightning Bolt', 'bolt', 1 - actor, 'selected')
            world.damage('bolt', 3, target='selected')
            world.check_deaths()
            world.receipts()
        if mode == 'noncombat_outgoing':
            if not p['untap']:
                world.witness_spell('Vitalize', 'vitalize', actor)
                for ref in attackers:
                    world.entities[ref]['tapped'] = False
            world.emit('choose_player', player=actor, value=1-actor)
            world.emit('activate', ref='selected', value=1)
            world.emit('assert_stack', value=1)
            world.emit('resolve')
            world.emit('assert_unused_choices', value=0)
            world.entities['selected']['tapped'] = True
            world.damage('selected', 1, player=1-actor)
            world.receipts()
        world.combat(attackers, blocked_target)
        if mode == 'cleanup':
            if real_expiry:
                world.advance_and_untap()
                world.advance_and_untap()
                world.set_actor(actor)
            else:
                world.cleanup()
                world.set_actor(actor)
                world.witness_spell('Vitalize', 'vitalize', actor)
                for ref in attackers:
                    world.entities[ref]['tapped'] = False
            world.declare(attackers)
            world.combat(attackers, None)
        return pending_cut, resolved_cut

    if no_combat:
        world.receipts()
    else:
        world.combat(attackers, blocked_target)
    return 0, 0


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    if not selected or len(set(selected)) != len(selected) or any(value not in variants(params) for value in selected):
        raise ValueError('explicit unique selected defense variants required')
    validate_witnesses(oracle, rows)
    results = []
    for variant in selected:
        mode, suffix = variant.rsplit('_', 1)
        world = DefenseWorld(name, params, int(suffix), rows, oracle)
        pending_cut, resolved_cut = _scenario(world, mode)
        steps = world.steps
        if mode in ('pending_snapshot', 'resolved_snapshot'):
            cut = pending_cut if mode == 'pending_snapshot' else resolved_cut
            tail = steps[cut:]
            steps = (steps[:cut] + [step('capture')] + copy.deepcopy(tail)
                     + [step('record_state'), step('ai_roundtrip'), step('restore')]
                     + copy.deepcopy(tail) + [step('assert_recorded_state')])
        results.append(dict(
            name=f'TargetedDamageDefenseRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
            setup=world.setup, steps=steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'],
                         untested_components=['land play actions, multiplayer, planeswalkers',
                                              'damage reassignment, unpreventable damage and multi-block assignment',
                                              'UI presentation staged at the actual declared combat boundary'])))
    return results
