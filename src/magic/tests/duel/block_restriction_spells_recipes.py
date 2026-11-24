"""Paid temporary block restrictions checked through real combat and resolution."""

import copy
import json
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld


FAMILY = 'block_restriction_spells'
PROFILE = Path(__file__).parent / 'data/block_restriction_spells_wave8_profiles.json'
GLOBAL = "Creatures without flying can't block this turn."
TARGETED = "Target creature can't block this turn.\nDraw a card."
MODAL = f'Choose one —\n• Destroy target artifact.\n• {GLOBAL}'
GRAMMAR = {GLOBAL: 'global', TARGETED: 'targeted', MODAL: 'modal'}
HANDLERS = {'global': 'falter', 'targeted': 'renegade_tactics', 'modal': 'destructive_tampering'}
COMMON = ('effect', 'expiry', 'short', 'wrong_color', 'countered', 'replay')
GLOBAL_VARIANTS = ('flying', 'reach', 'gain_flying', 'lose_flying', 'late_ground')
TARGETED_VARIANTS = ('flying_target', 'only_target', 'own_target', 'no_target', 'removed_target', 'one_card')
MODAL_VARIANTS = ('destroy', 'destroy_own', 'indestructible', 'destroy_removed', 'illegal_destroy', 'cancel')
INSTANT_VARIANTS = ('defend', 'preblock', 'afterblock')
ATTACKER = 'Grizzly Bears'
GROUND = 'Wall of Wood'
FLYING = 'Ornithopter'
REACH = 'Giant Spider'
LATE = 'Ambush Viper'
ARTIFACT = 'Fountain of Youth'
WITNESSES = {ATTACKER, GROUND, FLYING, REACH, LATE, ARTIFACT, 'Darksteel Relic', 'Wind Drake',
             'Flight', 'Grounded', 'Gravity Sphere', 'Gladecover Scout', 'Mountain', 'Island', 'Counterspell', 'Unsummon'}
DUMMIES = {ATTACKER, GROUND, FLYING, REACH, 'Wind Drake'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    mode = GRAMMAR.get(record['oracle_text'])
    if (not mode or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record['type_line'] not in ('Instant', 'Sorcery') or record['keywords']):
        raise ValueError('normal instant or sorcery with an exact complete block restriction required')
    if mode == 'modal' and record['type_line'] != 'Sorcery':
        raise ValueError('the artifact/blocking mode model requires sorcery timing')
    return dict(mode=mode, mana=cost_from_oracle(record), instant=record['type_line'] == 'Instant', keywords=[])


def handler(params):
    return 'manalinkeh_card_' + HANDLERS[params['mode']]


def variants(params):
    mode = params['mode']
    selected = COMMON + (TARGETED_VARIANTS if mode == 'targeted' else GLOBAL_VARIANTS)
    if mode == 'modal':
        selected += MODAL_VARIANTS
    if params['instant']:
        selected += INSTANT_VARIANTS
    return [f'{scenario}_{seat}' for scenario in selected for seat in (0, 1)]


def spec(declaration, record):
    params = classify(record)
    pin = profiles()['sources'].get(record['name'])
    proposed = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(params, sort_keys=True)):
        raise ValueError('typed parameters must match the complete pinned restriction spell')
    if (type(proposed) is not list or not proposed or any(type(value) is not str for value in proposed)
            or len(set(proposed)) != len(proposed) or not set(proposed) <= set(variants(params))):
        raise ValueError('approved distinct block restriction variants required')
    return dict(params, family=FAMILY, effect=FAMILY)


def validate_witnesses(oracle, registry):
    pins = profiles()
    if pins.get('version') != 1 or set(pins['witnesses']) != WITNESSES:
        raise ValueError('complete independent restriction witness set required')
    for group in ('sources', 'witnesses'):
        for name, pin in pins[group].items():
            row = registry.get(name, {})
            allowed_dummy = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != oracle.get(name) or pin['registry'] != row or row.get('kind') != 'card'
                    or row.get('handler') in ('NULL', None) or row['handler'] == 'card_dummy' and not allowed_dummy):
                raise ValueError(f'changed or unsupported restriction witness: {name}')
            if group == 'sources' and row['handler'] != handler(classify(oracle[name])):
                raise ValueError(f'changed restriction source handler: {name}')


class _World(_AttackResourceWorld):
    def __init__(self, name, params, owner, rows, oracle):
        super().__init__(name, params, owner, rows, oracle)
        self.foe = 1 - owner
        self.cards = {}
        self.restriction = None

    def add(self, name, ref, player, zone='battlefield'):
        super().add(name, ref, player, zone)
        self.cards[ref] = (name, player, zone)

    def move(self, ref, destination):
        name, player, zone = self.cards[ref]
        if ref in self.entities:
            if destination == 'battlefield':
                self.arrive(ref)
            else:
                self.leave(ref, destination)
        else:
            self.zones[zone][player].remove(name)
            self.zones[destination][player].append(name)
        self.cards[ref] = (name, player, destination)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref=ref, property=prop, value=int(value))

    def receipts(self):
        self.emit('refresh')
        super().receipts()
        for ref, (_, _, zone) in self.cards.items():
            if zone == 'battlefield' and ref not in self.entities:
                self.observe(ref, 'in_play', 1)

    def cast(self, ref, *, target=None, parent=None, dialog=None, denied=False, budget=None):
        from recipes import cost_from_oracle

        name, player, zone = self.cards[ref]
        if zone != 'hand':
            raise ValueError('cast requires a genuine hand card')
        cost = cost_from_oracle(self.o[name])
        self.pool(player, cost if budget is None else budget)
        self.emit('refresh')
        if dialog is not None:
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=player, value=dialog)
        if target:
            self.emit('choose_target', ref=target, player=player)
        self.emit('cast_response' if parent else 'cast', ref=ref, value=int(not denied))
        if dialog is not None:
            self.emit('present_ai_dialogs', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0 if denied else 2 if parent else 1)
        self.observe(ref, 'in_hand', denied)
        if not denied:
            self.mana[player] = [have - due for have, due in zip(self.mana[player], cost)]
        for wallet, amounts in enumerate(self.mana):
            for color, value in enumerate(amounts):
                self.emit('assert_mana', player=wallet, color=color, value=value)

    def finish(self, ref, zone, *, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.move(ref, zone)

    def use_source(self, scenario):
        from recipes import COLORS

        mode = self.p['mode']
        destructive = scenario.startswith('destroy') or scenario == 'indestructible'
        artifact = 'artifact' if destructive else None
        target = ('attacker' if scenario == 'own_target' else 'blocker') if mode == 'targeted' else artifact
        denied = scenario in ('short', 'wrong_color', 'no_target', 'illegal_destroy', 'cancel')
        budget = list(self.p['mana'])
        if scenario == 'short':
            budget[0 if budget[0] else COLORS['R']] -= 1
        elif scenario == 'wrong_color':
            budget[COLORS['R']] -= 1
            budget[COLORS['G']] += 1
        dialog = None
        if mode == 'modal' and scenario not in ('short', 'wrong_color'):
            dialog = 2 if scenario == 'cancel' else 0 if destructive or scenario == 'illegal_destroy' else 1
        self.cast('source', target=None if denied else target, dialog=dialog, denied=denied, budget=budget)
        if denied:
            self.receipts()
            return
        if scenario in ('countered', 'removed_target', 'destroy_removed'):
            self.emit('announce_spell', ref='source')
            self.cast('reply', target=None if scenario == 'countered' else target, parent='source')
            self.finish('reply', 'graveyard', response=True)
            if scenario == 'countered':
                self.move('source', 'graveyard')
            else:
                self.move(target, 'hand')
                self.finish('source', 'graveyard')
        else:
            self.finish('source', 'graveyard')
            if destructive:
                if 'Indestructible' not in self.o[self.cards[target][0]]['keywords']:
                    self.move(target, 'graveyard')
            elif mode == 'targeted':
                self.restriction = target
                self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))
            else:
                self.restriction = 'nonflying'
        self.receipts()

    def begin_combat(self, pairs):
        self.emit('begin_attackers')
        for attacker, _ in pairs:
            self.emit('attack', ref=attacker, value=1)
            self.entities[attacker].update(tapped=True, attacking=True)
        self.emit('finish_attackers')

    def block(self, pairs):
        self.emit('begin_blockers')
        accepted = {}
        for attacker, blocker in pairs:
            if self.cards[blocker][2] != 'battlefield':
                continue
            airborne = self.has(blocker, 'Flying')
            legal = (self.restriction != blocker and (self.restriction != 'nonflying' or airborne)
                     and (not self.has(attacker, 'Flying') or airborne or self.has(blocker, 'Reach')))
            self.emit('block', ref=blocker, other=attacker, value=int(legal))
            self.observe(blocker, 'blocking_count', legal)
            if legal:
                accepted[attacker] = blocker
        self.emit('advance_combat')
        for attacker, _ in pairs:
            self.observe(attacker, 'blocked', attacker in accepted)
        return accepted

    def damage(self, pairs, accepted):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for attacker, _ in pairs:
            attacking = self.entities[attacker]
            attacking['attacking'] = False
            if attacker in accepted:
                blocker = accepted[attacker]
                defending = self.entities[blocker]
                attacking['damage'] += defending['power']
                defending['damage'] += attacking['power']
                for ref, entity in ((attacker, attacking), (blocker, defending)):
                    if entity['damage'] >= entity['toughness']:
                        self.move(ref, 'graveyard')
            else:
                self.life[1 - self.current] -= attacking['power']
        self.receipts()

    def next_turn(self):
        self.restriction = None
        super().next_turn()


def fixture(w, scenario):
    mode = w.p['mode']
    w.current = w.foe if scenario == 'defend' else w.owner
    defender = 1 - w.current
    w.add(w.name, 'source', w.owner, 'hand')
    if mode == 'targeted':
        w.add('Mountain', 'top', w.owner, 'library')
        if scenario != 'one_card':
            w.add('Island', 'next', w.owner, 'library')
    if scenario == 'no_target':
        w.add('Gladecover Scout', 'protected', w.foe)
        return ()
    w.add('Wind Drake' if scenario == 'reach' else ATTACKER, 'attacker', w.current)
    if scenario in ('flying', 'lose_flying', 'flying_target'):
        w.add(FLYING, 'blocker', defender)
    elif scenario == 'reach':
        w.add(REACH, 'blocker', defender)
    elif scenario == 'late_ground':
        w.add(LATE, 'blocker', defender, 'hand')
    else:
        w.add(GROUND, 'blocker', defender)
    pairs = (('attacker', 'blocker'),)
    if scenario == 'only_target':
        w.add(ATTACKER, 'second_attacker', w.current)
        w.add(GROUND, 'second_blocker', defender)
        pairs += (('second_attacker', 'second_blocker'),)
    if scenario == 'lose_flying' and w.owner == 1:
        # Grounded's AI attachment revalidation requires its recipient to
        # retain flying. The world enchantment exercises the same transition.
        w.add('Gravity Sphere', 'sphere', w.owner, 'hand')
    elif scenario in ('gain_flying', 'lose_flying'):
        w.add('Flight' if scenario == 'gain_flying' else 'Grounded', 'aura', w.owner, 'hand')
    if scenario.startswith('destroy') or scenario in ('indestructible', 'cancel'):
        name = 'Darksteel Relic' if scenario == 'indestructible' else FLYING if scenario == 'destroy_removed' else ARTIFACT
        w.add(name, 'artifact', w.owner if scenario == 'destroy_own' else w.foe)
    if scenario in ('countered', 'removed_target', 'destroy_removed'):
        w.add('Counterspell' if scenario == 'countered' else 'Unsummon', 'reply', w.foe, 'hand')
    return pairs


def build_variant(name, params, scenario, owner, registry, oracle):
    from recipes import step

    w = _World(name, params, owner, registry, oracle)
    pairs = fixture(w, scenario)
    w.emit('turn', player=w.current, value=0)
    for seat in (0, 1):
        w.pool(seat, [0] * 6)
    w.receipts()
    if scenario == 'replay':
        w.emit('capture')
    cut = len(w.steps)
    if scenario in ('preblock', 'afterblock'):
        w.begin_combat(pairs)
    if scenario == 'afterblock':
        accepted = w.block(pairs)
    w.use_source(scenario)
    if scenario in ('gain_flying', 'lose_flying'):
        witness = 'sphere' if 'sphere' in w.cards else 'aura'
        w.cast(witness, target='blocker' if witness == 'aura' else None)
        w.finish(witness, 'battlefield')
        blocker = w.entities['blocker']
        if scenario == 'gain_flying':
            blocker['keys'].add('Flying')
        else:
            blocker['keys'].remove('Flying')
        w.receipts()
    elif scenario == 'late_ground':
        w.cast('blocker')
        w.finish('blocker', 'battlefield')
        w.receipts()
    if pairs:
        if scenario not in ('preblock', 'afterblock'):
            w.begin_combat(pairs)
        if scenario != 'afterblock':
            accepted = w.block(pairs)
        w.damage(pairs, accepted)
    if scenario == 'expiry':
        w.next_turn()
        w.next_turn()
        w.begin_combat(pairs)
        w.damage(pairs, w.block(pairs))
    if scenario == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'), step('ai_roundtrip'), step('restore'), *tail, step('assert_recorded_state')])
    return w.setup, w.steps


def expand(name, csvid, declaration, params, selected, registry, oracle):
    pin = profiles()['sources'].get(name)
    row = registry[name]
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(params):
        raise ValueError(f'unpinned restriction spell: {name}')
    cases = []
    for variant in selected:
        scenario, owner = variant.rsplit('_', 1)
        setup, steps = build_variant(name, params, scenario, int(owner), registry, oracle)
        gaps = ['separate priority passing before an otherwise legal action', 'multiplayer']
        cases.append(dict(name=f'BlockRestrictionSpellsRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=setup, steps=steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
