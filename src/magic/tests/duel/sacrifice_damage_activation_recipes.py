"""One fixed damage ability with a creature or artifact sacrifice cost."""
import copy
import json
import re
from pathlib import Path

from death_counter_trigger_recipes import exact
from death_modular_transfer_recipes import World as ModularWorld


FAMILY = 'sacrifice_damage_activation'
GAPS = ['independent opponent spell response to a pending activation',
        'AI discretionary sacrifice selection among different legal candidates']


def pins():
    return json.loads((Path(__file__).parent/'data/sacrifice_damage_activation_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or record.get('keywords')):
        raise ValueError('whole normal source with one fixed sacrifice damage ability required')
    types = record.get('type_line', '').split(' — ')[0].split()
    match = re.fullmatch(
        r'(?:(\{(?:[1-9]|[WUBRG])\}), )?(?:(\{T\}), )?Sacrifice (a creature|an artifact): '
        r'This (artifact|creature|enchantment) deals ([12]) damage to any target\.', record.get('oracle_text', ''))
    if not match or types != [match[4].capitalize()]:
        raise ValueError('whole reviewed printed source, costs, and any-target damage required')
    cost, tap, sacrifice, kind, amount = match.groups()
    return dict(activation_cost=cost or '', tap=bool(tap), sacrifice=sacrifice.split()[-1],
                kind=kind, damage=int(amount))


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = dict(schema=1, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or
            not exact(declaration.get('params'), expected)):
        raise ValueError('typed complete sacrifice damage declaration required')
    activation = cost_from_oracle({'mana_cost': expected['activation_cost']}) if expected['activation_cost'] else [0]*6
    model = dict(expected, family=FAMILY, mana=cost_from_oracle(record), activation=activation)
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct reviewed sacrifice damage variants required')
    return model


def variants(model):
    self_cost = model['kind'] == model['sacrifice']
    modes = ['activation', 'opponent_player', 'self_player', 'planeswalker', 'lethal', 'indestructible',
             'prevention', 'shroud_alternative', 'opposing_turn', 'cast_payment', 'cast_unaffordable', 'countered',
             'bare_zones', 'ready_snapshot', 'pending_snapshot', 'resolved_snapshot']
    if not self_cost:
        modes += ['no_fodder', 'wrong_type', 'enemy_only', 'stolen_cost', 'token_cost', 'fresh_activation']
        if model['sacrifice'] == 'artifact': modes += ['artifact_creature_cost']
    if model['tap']:
        modes += ['tapped']
        if model['kind'] == 'creature': modes += ['fresh_denied']
    if model['activation_cost']:
        modes += ['activation_unaffordable']
        if any(model['activation'][1:]): modes += ['activation_wrong_color']
    result = [f'{mode}_{p}' for p in (0, 1) for mode in modes]
    if self_cost:
        result += ['self_cost_0', 'self_cost_1', 'stolen_cost_0', 'token_cost_0']
    if model['tap']:
        result += ['untap_repeat_0'] if self_cost else ['untap_repeat_0', 'untap_repeat_1']
    else:
        result += ['repeat_0', 'repeat_1']
    if not model['tap'] and not model['activation_cost']:
        result += ['cancel_cost_0', 'cancel_damage_target_0']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record, oracle.get(name)) or not exact(data['registry'][name], registry.get(name)):
            raise ValueError('sacrifice damage witness changed: '+name)
    for name, pin in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(row, pin['registry']) or not exact(pin['oracle'], oracle.get(name)) or row.get('kind') != 'card' or
                row.get('handler') in ('NULL', '0', 'card_dummy') or type(pin['sacrifice_first']) is not bool):
            raise ValueError('implemented normal sacrifice damage source changed: '+name)
        oracle_model(oracle[name])


class World(ModularWorld):
    def __init__(self, *args, sacrifice_first, **kwargs):
        super().__init__(*args, **kwargs)
        self.forced_targets = 0
        self.sacrifice_first = sacrifice_first
        self.loyalty = {}

    def stats(self, key='source'):
        from recipes import step

        checks = super().stats(key)
        if key in self.loyalty and self.entities[key]['alive']:
            checks.append(step('assert_counter', ref=self.entities[key]['ref'], counter_kind='loyalty', value=self.loyalty[key]))
        return checks

    def cast_source(self):
        from recipes import step

        p = self.controller
        if self.model['kind'] == 'creature':
            return self.cast_creature('source', self.model['mana'])
        self.zones['hand'][p].remove(self.name)
        self.source['alive'] = True
        self.mana[p] = [0]*6
        return [step('cast', ref='source', value=1), step('assert_stack', value=1),
                step('resolve'), step('assert_choices_consumed')]

    def pending(self, victim, target_player=None, target='recipient'):
        from recipes import step

        p = self.controller
        victim_choice = [step('choose_target', player=p, ref=victim)] if p == 0 else []
        attack_choice = [step('choose_player', player=p, value=target_player)] if target_player is not None else [
            step('choose_target', player=p, ref=target)]
        choices = victim_choice + attack_choice if self.sacrifice_first else attack_choice + victim_choice
        run = [step('require_can_activate', ref='source', value=1)] + choices + [
            step('activate', ref='source', value=1), step('assert_stack', value=1), step('assert_choices_consumed')]
        self.mana[p] = [held-spent for held, spent in zip(self.mana[p], self.model['activation'])]
        if self.model['tap']:
            self.source['tapped'] = 1
        self.leave(victim)
        removed = self.entities[victim]
        owner = removed['owner']; card_name = removed['name']
        receipt = self.zones['graveyard'][owner].count(card_name)
        run += [step('assert_zone_multiplicity', player=owner, zone='graveyard', index=self.registry[card_name]['csvid'], value=receipt),
                step('assert_zone', player=removed['player'], zone='battlefield', value=sum(
                    e['alive'] and e['player'] == removed['player'] for e in self.entities.values()))]
        return run+self.effects()

    def resolve_damage(self, target_player=None, target='recipient', prevent=False):
        from recipes import step

        amount = 0 if prevent else self.model['damage']
        if target_player is not None:
            self.life[target_player] -= amount
        elif target in self.loyalty:
            self.loyalty[target] -= amount
            if self.loyalty[target] <= 0: self.leave(target)
        else:
            entity = self.entities[target]
            entity['damage'] += amount
            record = self.oracle[entity['name']]
            if entity['damage'] >= int(record['toughness']) and 'Indestructible' not in record.get('keywords', []):
                self.leave(target)
        return [step('resolve'), step('assert_choices_consumed'), step('assert_stack', value=0)]+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, cost_from_oracle

    validate_witnesses(oracle, registry)
    pin = pins()['sources'].get(name)
    if not pin or pin['registry']['csvid'] != csvid or not selected or set(selected)-set(variants(model)):
        raise ValueError('unreserved source or unsupported sacrifice damage selection')
    result = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        p = int(player); q = 1-p
        world = World(name, p, model, registry, oracle, bare=mode == 'bare_zones', sacrifice_first=pin['sacrifice_first'])
        self_eligible = model['kind'] == model['sacrifice']
        pays_self = mode == 'self_cost' or (self_eligible and p == 1)
        from_hand = mode in ('cast_payment', 'cast_unaffordable', 'countered', 'fresh_activation', 'fresh_denied')
        world.add(name, 'source', p, 'hand' if from_hand else 'battlefield')
        world.source['tapped'] = int(mode == 'tapped')
        if mode == 'tapped':
            next(c for c in world.setup if c['ref'] == 'source')['state'] = ['tapped']
        target_player = {'opponent_player':q, 'self_player':p, 'shroud_alternative':q, 'self_cost':q}.get(mode)
        victim = 'source' if pays_self else 'fodder'
        matching = 'Muck Rats' if model['sacrifice'] == 'creature' else 'Howling Mine'
        wrong_type = 'Howling Mine' if model['sacrifice'] == 'creature' else 'Muck Rats'
        if not pays_self and mode not in ('no_fodder', 'cast_payment', 'cast_unaffordable', 'countered'):
            fodder = {'wrong_type':wrong_type, 'artifact_creature_cost':'Wall of Spears'}.get(mode, matching)
            world.add(fodder, 'fodder', q if mode == 'enemy_only' else p, foreign=mode == 'stolen_cost', token=mode == 'token_cost')
        if mode in ('repeat', 'untap_repeat'):
            world.add(matching, 'next_fodder', p, 'hand')
        if mode not in ('cast_payment', 'cast_unaffordable', 'countered') and (target_player is None or mode == 'shroud_alternative'):
            target_name = {'lethal':'Muck Rats', 'indestructible':'Darksteel Myr', 'planeswalker':'Chandra, Pyrogenius'}.get(mode, 'Craw Wurm')
            world.add(target_name, 'recipient', q)
            if mode == 'planeswalker': world.loyalty['recipient'] = int(oracle[target_name]['loyalty'])
        actions = []
        ready_cut = pending_cut = None
        if mode == 'cast_unaffordable':
            actions += world.fund(p, [0]*6)+[step('cast', ref='source', value=0)]
        elif mode == 'countered':
            actions += world.cast_creature('source', model['mana'], resolve=False)
            world.zones['graveyard'][p].append(name)
            world.add('Counterspell', 'counter', q, 'hand')
            actions += [step('announce_spell', ref='source')]+world.fund(q,cost_from_oracle(oracle['Counterspell']))+[
                step('cast_response', ref='counter', value=1), step('resolve_response')]
            world.zones['hand'][q].remove('Counterspell'); world.zones['graveyard'][q].append('Counterspell')
            world.mana[q] = [0]*6
        else:
            if from_hand:
                actions += world.fund(p, model['mana'])+world.cast_source()+world.effects()
            if mode == 'fresh_activation' and model['tap'] and model['kind'] == 'creature':
                actions += world.age()
            if mode == 'prevention':
                actions += world.spell('Safe Passage', 'shield', caster=q)+[step('resolve'),step('assert_choices_consumed')]
            if mode == 'shroud_alternative':
                actions += world.spell('Robe of Mirrors','shroud',('recipient',),caster=q)+[step('resolve'),step('assert_choices_consumed')]
                world.entities['shroud']['attached'] = 'recipient'
            if mode == 'opposing_turn':
                actions += [step('turn', player=q, value=0)]
            if mode != 'cast_payment':
                pool = list(model['activation'])
                if mode == 'activation_unaffordable': pool = [0]*6
                if mode == 'activation_wrong_color': pool = [0,0,1,0,0,0]
                actions += world.fund(p, pool)+[step('refresh')]
                ready_cut = len(actions)
                if mode in ('no_fodder','wrong_type','enemy_only','tapped','fresh_denied','activation_unaffordable','activation_wrong_color'):
                    actions += [step('require_can_activate', ref='source', value=0), step('activate', ref='source', value=0)]
                elif mode in ('cancel_cost', 'cancel_damage_target'):
                    if mode == 'cancel_damage_target': actions += [step('choose_target', player=p, ref=victim)]
                    actions += [step('choose_cancel_target', player=p), step('activate', ref='source', value=0)]
                else:
                    actions += world.pending(victim, target_player)
                    pending_cut = len(actions)
                    actions += world.resolve_damage(target_player, prevent=mode == 'prevention')
                    if mode in ('repeat', 'untap_repeat'):
                        if mode == 'untap_repeat':
                            actions += world.age()
                        # Enter a second legal cost only after the first is paid.
                        # This avoids depending on the AI's choice of object.
                        fodder_mana = cost_from_oracle(oracle[matching])
                        actions += world.cast_creature('next_fodder', fodder_mana)+world.effects()
                        actions += world.fund(p, model['activation'])+world.pending('next_fodder', target_player)
                        actions += world.resolve_damage(target_player)
        checks = world.receipts()+world.effects()
        sequence = world.pre+actions+checks
        if mode in ('ready_snapshot','pending_snapshot'):
            cut = ready_cut if mode == 'ready_snapshot' else pending_cut
            if cut is None: raise ValueError('real activation replay boundary missing')
            tail = actions[cut:]+checks
            sequence = (world.pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+
                        [step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')])
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'SacrificeDamageActivationRecipe_{csvid}_{variant}', card=name,
                           fixture='empty_main', seed=123, setup=world.setup, steps=sequence,
                           _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                        oracle_text=declaration['oracle_text'], scope_gaps=GAPS)))
    return result
