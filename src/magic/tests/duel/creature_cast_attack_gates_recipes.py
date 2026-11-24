"""Attacker legality from actual controller-specific cast and combat history."""

from copy import deepcopy
from functools import lru_cache
import hashlib
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene
from recipes import cost_from_oracle, step


FAMILY = 'creature_cast_attack_gates'
PROFILE = Path(__file__).with_name('data') / 'creature_cast_attack_gates_wave8_profiles.json'
CLAUSES = {
    'creature': "This creature can't attack unless you've cast a creature spell this turn.",
    'noncreature': "This creature can't attack unless you've cast a noncreature spell this turn.",
    'last_attack': "This creature can't attack if it attacked during your last turn.",
    'arbiter': ("Flying\nEach opponent who cast a spell this turn can't attack with creatures.\n"
                "Each opponent who attacked with a creature this turn can't cast spells."),
}
PAYMENT = ('short', 'wrong_color', 'fresh')
CAST_GATE = ('no_cast', 'match', 'mismatch', 'countered_match', 'opponent_cast', 'prior_turn', 'failed_cast',
             'land', 'creature_land', 'self_cast_haste', 'blocking', 'snapshot')
TURTLE = ('ready', 'consecutive', 'skip_then_resume', 'other_attacked', 'failed_attack', 'blocking', 'snapshot')
ARBITER = ('idle', 'opponent_cast', 'opponent_creature', 'countered_cast', 'owner_cast', 'land_before',
           'attack_then_instant', 'attack_then_land', 'other_cast_after_attack', 'failed_attack',
           'turn_reset', 'flying', 'snapshot')
MODES = {'creature': CAST_GATE, 'noncreature': CAST_GATE, 'last_attack': TURTLE, 'arbiter': ARBITER}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ')):
        raise ValueError('complete original creature required')
    effect = next((kind for kind, clause in CLAUSES.items() if record.get('oracle_text') == clause), None)
    if effect is None or record.get('keywords') != (['Flying'] if effect == 'arbiter' else []):
        raise ValueError('complete reviewed attack gate required')
    return dict(effect=effect, cast_mana=cost_from_oracle(record), power=int(record['power']),
                toughness=int(record['toughness']))


def variants(model):
    return [f'{mode}_{player}' for mode in PAYMENT + MODES[model['effect']] for player in (0, 1)]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    pin = profiles()['sources'].get(record['name'], {})
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('whole pinned attack gate and explicit distinct variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unused = json.loads((PROFILE.parent / 'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented = {entry['csvid'] for entry in unused.values()}
    root = Path(__file__).resolve().parents[4]
    for name, pin in (data['sources'] | data['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(pin['oracle'], oracle.get(name)) or not exact(pin['registry'], row) or row.get('kind') != 'card'
                or row['csvid'] in unimplemented or row['handler'] in (None, 'NULL', '0', 'card_dummy')
                or row.get('layout', 'normal') != 'normal'):
            raise ValueError('attack source or active witness changed: ' + name)
        source = pin.get('compiled_source')
        if source is not None and hashlib.sha256((root / source['path']).read_bytes()).hexdigest() != source['sha256']:
            raise ValueError('compiled attack restriction changed: ' + name)


class AttackGateScene(Scene):
    def setup_cards(self):
        mode, shape = self.mode, self.effect
        self.add(self.name, 'source', self.owner, 'hand' if mode in PAYMENT + ('self_cast_haste',) else 'battlefield',
                 tapped=shape == 'last_attack' and mode == 'failed_attack')
        if mode == 'self_cast_haste':
            self.add('Mass Hysteria', 'haste', self.owner)
        if shape in ('creature', 'noncreature'):
            self.setup_cast_gate()
        elif shape == 'last_attack':
            if mode in ('other_attacked', 'blocking'):
                self.add('Elvish Mystic', 'actor', self.owner if mode == 'other_attacked' else self.foe)
        elif mode not in PAYMENT + ('flying',):
            self.setup_arbiter()
            if self.owner == 1 and mode not in ('idle', 'owner_cast', 'failed_attack'):
                # The native baseline starts on seat zero. Establish its actor
                # before the Arbiter for the functional cast and combat cases.
                # The idle contrast retains the actual noncast-entry path.
                self.setup.insert(0, self.setup.pop(1))

    def setup_cast_gate(self):
        mode, creature = self.mode, self.effect == 'creature'
        if mode == 'blocking':
            self.add('Elvish Mystic', 'actor', self.foe)
            return
        if mode in ('land', 'creature_land'):
            self.add('Dryad Arbor' if mode == 'creature_land' else 'Forest', 'land', self.owner, 'hand')
            return
        if mode in ('match', 'countered_match', 'prior_turn', 'failed_cast', 'snapshot'):
            self.add('Elvish Mystic' if creature else 'Shock', 'witness', self.owner, 'hand')
            if mode == 'countered_match':
                self.add('Counterspell', 'reply', self.foe, 'hand')
        elif mode == 'mismatch':
            self.add('Shock' if creature else 'Elvish Mystic', 'witness', self.owner, 'hand')
        elif mode == 'opponent_cast':
            self.add('Ambush Viper' if creature else 'Shock', 'witness', self.foe, 'hand')

    def setup_arbiter(self):
        mode = self.mode
        self.add('Elvish Mystic', 'actor', self.owner if mode == 'owner_cast' else self.foe,
                 tapped=mode == 'failed_attack')
        if mode in ('opponent_cast', 'countered_cast', 'snapshot', 'owner_cast'):
            self.add('Shock', 'witness', self.owner if mode == 'owner_cast' else self.foe, 'hand')
        if mode == 'opponent_creature':
            self.add('Ambush Viper', 'witness', self.foe, 'hand')
        if mode == 'countered_cast':
            self.add('Counterspell', 'reply', self.owner, 'hand')
        if mode in ('land_before', 'attack_then_land'):
            self.add('Forest', 'land', self.foe, 'hand')
        if mode in ('attack_then_instant', 'other_cast_after_attack', 'failed_attack'):
            caster = self.owner if mode == 'other_cast_after_attack' else self.foe
            self.add('Reach Through Mists', 'instant', caster, 'hand')
            self.add('Island', 'top', caster, 'library')
            self.add('Plains', 'tail', caster, 'library')
        if mode == 'turn_reset':
            self.add('Darksteel Relic', 'attempt', self.foe, 'hand')

    def phase_start(self):
        if self.mode == 'flying':
            self.add('Elvish Mystic', 'ground', self.foe)
            self.add('Nessian Asp', 'reach', self.foe)
        if self.mode == 'blocking' or self.effect == 'arbiter' and self.mode not in PAYMENT + ('owner_cast','flying'):
            self.active = self.foe
        self.emit('turn', player=self.active, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')

    def begin_cast(self, ref, *, allowed=True, response=False, budget=None):
        entity = self.entities[ref]
        actor, name = entity['owner'], entity['name']
        oracle = self.oracle[name]
        price = [0]*6 if oracle['mana_cost'] == '{0}' else cost_from_oracle(oracle)
        self.fund(actor, price if budget is None else budget)
        if allowed and name == 'Shock':
            self.emit('choose_player', player=actor, value=1-actor)
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_stack', value=(2 if response else 1) if allowed else 0)
        self.emit('assert_unused_choices', value=0)
        if allowed:
            self.prop(ref, 'in_hand', 0)
            self.pay(actor, price)
            entity['zone'] = 'stack'
            self.emit('announce_spell', ref=ref)
        self.pools()

    def place_result(self, ref, countered=False):
        entity = self.entities[ref]
        name, actor = entity['name'], entity['owner']
        line = self.oracle[name]['type_line'].split(' — ', 1)[0].split()
        if not countered and any(part in line for part in ('Artifact', 'Creature', 'Enchantment')):
            entity['zone'] = 'battlefield'
        else:
            self.leave(ref, 'graveyard')
        if not countered and name == 'Shock':
            self.life[1-actor] -= 2
        if not countered and name == 'Reach Through Mists':
            self.leave('top', 'hand')
            self.emit('assert_zone_card', player=actor, zone='library', index=0, value=self.rows['Plains']['csvid'])

    def cast(self, ref, *, allowed=True, countered=False, budget=None):
        self.begin_cast(ref, allowed=allowed, budget=budget)
        if countered:
            self.begin_cast('reply', response=True)
        # Resolve handles failed attempts too, so a newly accepted targetless
        # spell reports state differences instead of leaving a stale stack.
        self.resolve(parent=countered)
        if allowed:
            self.place_result(ref, countered)
        if countered:
            self.place_result('reply')
        self.receipts()

    def land(self):
        e = self.entities['land']
        self.emit('play_land', ref='land', value=1)
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        e['zone'] = 'battlefield'
        self.receipts()

    def next_turn(self):
        self.advance()
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.emit('turn', player=self.active, value=0)
        for e in self.entities.values():
            if e['controller'] == self.active and e['zone'] == 'battlefield':
                e['tapped'] = False
        self.receipts()

    def own_next_turn(self):
        self.next_turn()
        self.next_turn()

    def begin_attack(self, ref, allowed):
        self.emit('begin_attackers')
        self.emit('attack', ref=ref, value=int(allowed))
        self.prop(ref, 'attacking', allowed)
        if allowed:
            self.entities[ref]['tapped'] = True
        self.prop(ref, 'tapped', self.entities[ref]['tapped'])
        self.emit('finish_attackers')

    def finish_combat(self, ref, allowed, blocker=None):
        self.emit('begin_blockers')
        if blocker:
            if self.mode == 'flying':
                self.emit('block', ref='ground', other=ref, value=0)
            self.emit('block', ref=blocker, other=ref, value=1)
        self.emit('advance_combat')
        if allowed:
            self.prop(ref, 'blocked', bool(blocker))
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if allowed:
            if blocker:
                self.entities[ref]['damage'] += self.stats(blocker)[0]
                self.entities[blocker]['damage'] += self.stats(ref)[0]
                for locator in (ref, blocker):
                    if self.entities[locator]['damage'] >= self.stats(locator)[1]:
                        self.leave(locator, 'graveyard')
            else:
                self.life[1-self.active] -= self.stats(ref)[0]
        self.receipts()

    def combat(self, ref, allowed, blocker=None):
        self.begin_attack(ref, allowed)
        self.finish_combat(ref, allowed, blocker)

    def cast_payment(self):
        mode = self.mode
        if mode == 'fresh':
            self.cast('source')
            self.prop('source', 'sick', 1)
            self.combat('source', False)
        else:
            fee = list(self.model['cast_mana'])
            color = next(i for i, amount in enumerate(fee) if amount)
            fee[color] -= 1
            if mode == 'wrong_color':
                wrong = next(i for i in range(1, 6) if not self.model['cast_mana'][i])
                # Replace a required colored unit, even when a generic
                # component is present on the printed source.
                fee = list(self.model['cast_mana'])
                needed = next(i for i, amount in enumerate(fee) if i and amount)
                fee[needed] -= 1
                fee[wrong] += 1
            self.cast('source', allowed=False, budget=fee)

    def cast_gate(self):
        mode = self.mode
        if mode == 'blocking':
            self.combat('actor', True, 'source')
            return
        if 'witness' in self.entities:
            self.cast('witness', allowed=mode != 'failed_cast', countered=mode == 'countered_match',
                      budget=[0]*6 if mode == 'failed_cast' else None)
        if mode == 'prior_turn':
            self.own_next_turn()
        if mode in ('land','creature_land'):
            self.land()
        if mode == 'self_cast_haste':
            self.cast('source')
        if mode == 'snapshot':
            self.replay = len(self.steps)
        allowed = mode in ('match','countered_match','snapshot') or mode == 'self_cast_haste' and self.effect == 'creature'
        self.combat('source', allowed)

    def turtle(self):
        mode = self.mode
        if mode == 'blocking':
            self.combat('actor', True, 'source')
            self.next_turn()
            self.combat('source', True)
        elif mode == 'other_attacked':
            self.combat('actor', True)
            self.own_next_turn()
            self.combat('source', True)
        elif mode == 'failed_attack':
            self.combat('source', False)
            self.own_next_turn()
            self.combat('source', True)
        else:
            self.combat('source', True)
            if mode != 'ready':
                if mode == 'snapshot':
                    self.replay = len(self.steps)
                self.own_next_turn()
                self.prop('source', 'tapped', 0)
                self.combat('source', False)
                if mode == 'skip_then_resume':
                    self.own_next_turn()
                    self.combat('source', True)

    def arbiter(self):
        mode = self.mode
        if self.owner == 1 and mode in ('attack_then_instant', 'other_cast_after_attack'):
            # Reach Through Mists makes fixture preparation install reserved
            # rules permanents. Start after two actual turn transitions so
            # Arbiter sees a clean opponent turn before the first attack.
            self.advance()
            self.advance()
        if mode == 'flying':
            self.combat('source', True, 'reach')
            return
        if 'witness' in self.entities:
            self.cast('witness', countered=mode == 'countered_cast')
        if mode == 'land_before':
            self.land()
        if mode == 'snapshot':
            self.replay = len(self.steps)
        allowed = mode not in ('opponent_cast','opponent_creature','countered_cast','failed_attack','snapshot')
        self.begin_attack('actor', allowed)
        if 'instant' in self.entities:
            self.cast('instant', allowed=mode != 'attack_then_instant')
        self.finish_combat('actor', allowed)
        if mode == 'attack_then_land':
            self.land()
        if mode == 'turn_reset':
            self.cast('attempt', allowed=False)
            self.own_next_turn()
            self.cast('attempt')
            self.combat('actor', False)

    def play(self):
        self.setup_cards()
        self.phase_start()
        if self.mode in PAYMENT:
            self.cast_payment()
        elif self.effect in ('creature','noncreature'):
            self.cast_gate()
        elif self.effect == 'last_attack':
            self.turtle()
        else:
            self.arbiter()


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if (not selected or len(set(selected)) != len(selected) or not set(selected) <= set(variants(model))
            or csvid != profiles()['sources'][name]['registry']['csvid']):
        raise ValueError('unreserved attack gate or unreviewed variant')
    cases = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        scene = AttackGateScene(name, model, mode, int(actor), rows, oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            tail = steps[cut:]
            steps = steps[:cut] + [step('capture')] + tail + [step('record_state'),step('ai_roundtrip'),step('restore')] + deepcopy(tail) + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=['Multiplayer and attack redirection to third parties.'])
        cases.append(dict(name=f'CreatureCastAttackGatesRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=123, setup=scene.setup, steps=steps, _recipe=metadata))
    return cases
