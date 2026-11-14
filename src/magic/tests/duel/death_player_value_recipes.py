"""Whole fixed death-to-player effects through real deaths and source legacies."""
import copy
import json
from pathlib import Path
import re

from death_counter_extended_recipes import World as ZoneWorld
from death_counter_trigger_recipes import exact

FAMILY = 'death_player_value'


def pins():
    return json.loads((Path(__file__).parent/'data/death_player_value_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or record.get('keywords')):
        raise ValueError('whole normal single-clause fixed death-player source required')
    types = set(record.get('type_line', '').partition(' \u2014 ')[0].split())
    if not types & {'Creature', 'Artifact', 'Enchantment'} or 'Aura' in record.get('type_line', ''):
        raise ValueError('standalone death-player permanent required')
    if 'Creature' in types and any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('numeric surviving source creature required')
    text = record.get('oracle_text', '')
    if '\n' in text:
        raise ValueError('additional source clauses need a separate model')
    head, sep, effect = text.partition(', ')
    if not sep:
        raise ValueError('complete triggered clause required')
    predicate = None
    if m := re.fullmatch(r'Whenever (a|another) creature( you control)? dies', head):
        predicate = dict(kind='creature', another=m[1] == 'another', controller='you' if m[2] else 'any', owner='any', nontoken=False)
    elif m := re.fullmatch(r'Whenever (another )?artifact is put into a graveyard from the battlefield', head):
        predicate = dict(kind='artifact', another=bool(m[1]), controller='any', owner='any', nontoken=False)
    elif head == 'Whenever another Goblin you control is put into a graveyard from the battlefield':
        predicate = dict(kind='goblin_permanent', another=True, controller='you', owner='any', nontoken=False)
    elif head == 'Whenever an artifact or enchantment is put into your graveyard from the battlefield':
        predicate = dict(kind='artifact_or_enchantment', another=False, controller='any', owner='you', nontoken=False)
    elif head == "Whenever a nontoken permanent is put into a player's graveyard from the battlefield":
        predicate = dict(kind='permanent', another=False, controller='any', owner='any', nontoken=True)
    if predicate is None:
        raise ValueError('unreviewed death subject')
    optional = effect.startswith('you may have ')
    body = effect.removeprefix('you may have ')
    damage_verb = 'deal' if optional else 'deals'
    m = re.fullmatch(r'(this creature|this artifact|this enchantment) '+damage_verb+
                    r" (\d+) damage to (each opponent|that creature's controller|that artifact's controller|target player or planeswalker)(?: and you gain (\d+) life)?\.", body)
    if m:
        if m[1].split()[1].capitalize() not in types:
            raise ValueError('damage source type must match the complete Oracle record')
        recipient = {'each opponent': 'opponents', "that creature's controller": 'dead_controller',
                     "that artifact's controller": 'dead_controller', 'target player or planeswalker': 'target'}[m[3]]
        receipt = dict(effect='damage', amount=int(m[2]), recipient=recipient, gain=int(m[4] or 0), optional_effect=optional,
                       mandatory_target=recipient == 'target')
    elif not optional and (m := re.fullmatch(r'that player loses (\d+) life\.', effect)):
        if predicate['kind'] != 'permanent' or not predicate['nontoken']:
            raise ValueError('that player must be the printed graveyard recipient')
        receipt = dict(effect='loss', amount=int(m[1]), recipient='graveyard_owner', gain=0, optional_effect=False, mandatory_target=False)
    elif not optional and (m := re.fullmatch(r'you gain (\d+) life\.', effect)):
        receipt = dict(effect='gain', amount=int(m[1]), recipient='you', gain=0, optional_effect=False, mandatory_target=False)
    else:
        raise ValueError('whole fixed player damage/loss/gain clause required')
    if not 1 <= receipt['amount'] <= 3 or not 0 <= receipt['gain'] <= 3:
        raise ValueError('reviewed bounded fixed per-death receipt required')
    return dict(predicate=predicate, **receipt)


def spec(declaration, record):
    from recipes import cost_from_oracle
    expected = dict(schema=1, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(declaration.get('params'), expected)):
        raise ValueError('typed complete fixed death-player declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record),
                 source_creature='Creature' in record['type_line'],
                 power=int(record.get('power', 0)), toughness=int(record.get('toughness', 0)))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported death-player variants required')
    return model


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'countered', 'effect', 'opponent_death',
             'stolen_death', 'lent_death', 'source_foreign_owner', 'token_death', 'nonmatching',
             'artifact_death', 'enchantment_death', 'land_death', 'source_only', 'source_removed',
             'source_bounced', 'source_exiled', 'source_blinked', 'source_simultaneous',
             'victim_bounced', 'victim_exiled', 'repeat', 'two_deaths', 'prevented', 'cleanup_once',
             'lethal_death', 'zero_toughness', 'combat_death', 'bare_zones',
             'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
    if any(model['mana'][1:]):
        modes.append('wrong_color')
    else:
        modes.append('generic_colored_payment')
    if model['mandatory_target']:
        modes += ['target_self_player', 'planeswalker', 'own_planeswalker', 'split_targets', 'player_shroud']
    if model['predicate']['kind'] == 'goblin_permanent':
        modes += ['kindred_goblin', 'changeling']
    result = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    if model['mandatory_target']:
        result += ['mandatory_policy_0']
    if model['optional_effect']:
        result += ['decline_0', 'partial_accept_0']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record, oracle.get(name)) or not exact(data['registry'][name], registry.get(name)):
            raise ValueError('complete death-player witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(source['oracle'], oracle.get(name)) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('NULL', 'card_dummy', '0')):
            raise ValueError('implemented whole death-player source changed: '+name)
        model = oracle_model(oracle[name])
        presentation = data['presentation'].get(name)
        if (not isinstance(presentation, dict) or set(presentation) != {'optional_outer', 'live_source_selector', 'dead_legacy_selector'} or
                presentation['optional_outer'] not in ('ai', 'mandatory') or
                any(type(presentation[k]) is not bool for k in ('live_source_selector', 'dead_legacy_selector')) or
                not model['optional_effect'] and presentation != dict(optional_outer='mandatory', live_source_selector=False, dead_legacy_selector=False)):
            raise ValueError('actual source/legacy menu presentation required: '+name)


class World(ZoneWorld):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.forced_targets, self.death_cut = 0, None
        self.prevented = set()
        self.target = 1-self.controller

    def stats(self, key='source'):
        from recipes import step
        entity = self.entities[key]
        checks = super().stats(key)
        if entity['alive'] and 'loyalty' in entity:
            checks += [step('assert_counter', ref=entity['ref'], counter_kind='loyalty', value=entity['loyalty'])]
        return checks

    def effects(self):
        from recipes import step
        return ([s for key, e in self.entities.items() if e['alive'] for s in self.stats(key)]+
                [step('assert_life', player=p, value=self.life[p]) for p in (0, 1)]+
                [step('assert_unused_choices', value=0), step('assert_forced_target_choices', value=self.forced_targets)]+
                self.mana_checks())

    def qualifies(self, key):
        entity = self.entities[key]
        p = self.model['predicate']
        if not entity['alive'] or p['another'] and key == 'source' or p['nontoken'] and entity['token']:
            return False
        if p['controller'] == 'you' and entity['player'] != self.controller or p['owner'] == 'you' and entity['owner'] != self.controller:
            return False
        record = self.oracle[entity['name']]
        types = set(record['type_line'].partition(' \u2014 ')[0].split())
        subtype = record['type_line'].partition(' \u2014 ')[2].split()
        return {'creature': 'Creature' in types, 'artifact': 'Artifact' in types,
                'artifact_or_enchantment': bool(types & {'Artifact', 'Enchantment'}),
                'permanent': bool(types & {'Creature', 'Artifact', 'Enchantment', 'Land', 'Planeswalker'}),
                'goblin_permanent': 'Goblin' in subtype or 'Changeling' in record.get('keywords', [])}[p['kind']]

    def cast_source(self):
        from recipes import observe, step
        p = self.controller
        payment = ([step('choose_mana', player=p, color=c) for c in range(1, 6) for _ in range(self.mana[p][c])]
                   if p == 0 and not any(self.model['mana'][1:]) else [])
        self.zones['hand'][p].remove(self.name)
        self.source['alive'] = True
        self.mana[p] = [a-b for a, b in zip(self.mana[p], self.model['mana'])] if any(self.model['mana'][1:]) else [0]*6
        return payment+[step('cast', ref='source', value=1), step('assert_stack', value=1), observe('source', 'in_hand', 0)]

    def outcome(self, key, target, decline=False):
        if decline:
            return
        m, entity = self.model, self.entities[key]
        recipient = {'you': self.controller, 'opponents': 1-self.controller,
                     'dead_controller': entity['player'], 'graveyard_owner': entity['owner'], 'target': target}[m['recipient']]
        if isinstance(recipient, str):
            walker = self.entities[recipient]
            if walker['player'] not in self.prevented:
                walker['loyalty'] -= m['amount']
                if walker['loyalty'] <= 0:
                    self.leave(recipient)
        elif m['effect'] == 'gain':
            self.life[recipient] += m['amount']
        elif m['effect'] == 'loss' or recipient not in self.prevented:
            self.life[recipient] -= m['amount']
        self.life[self.controller] += m['gain']

    def intent(self, keys, *, decline=False, partial=False, policy=False, split=False):
        from recipes import step
        if not self.source['alive']:
            return []
        matches = [key for key in keys if self.qualifies(key)]
        if not matches:
            return []
        run = []
        dying = 'source' in keys
        selector = self.presentation['dead_legacy_selector' if dying else 'live_source_selector']
        if selector and self.controller == 0:
            run += [step('choose_trigger' if dying else 'choose_trigger_source', ref=self.source['ref'], player=self.controller)]
        for n, key in enumerate(matches):
            skip = decline or partial and n > 0
            target = 'walker' if (skip or policy or split and n > 0) else self.target
            if self.model['mandatory_target']:
                if skip or policy:
                    run += [step('choose_optional_target', ref='walker', player=self.controller)]
                    self.forced_targets += 1
                elif isinstance(target, str):
                    run += [step('choose_target', ref=self.entities[target]['ref'], player=self.controller)]
                else:
                    run += [step('choose_player', player=self.controller, value=target)]
            self.outcome(key, target, decline=skip)
        return run

    def kill(self, key='victim', spell_key='death_spell', spell=None, **choices):
        from recipes import step
        record = self.oracle[self.entities[key]['name']]
        creature = 'Creature' in record['type_line']
        land = 'Land' in record['type_line'].partition(' \u2014 ')[0].split()
        removal = spell or ('Murder' if creature else 'Stone Rain' if land else 'Naturalize')
        run = self.spell(removal, spell_key, (key,), caster=self.controller if land else 1-self.controller)
        self.death_cut = len(self.actions)+len(run)
        run += self.intent([key], **choices)
        self.leave(key)
        return run+[step('resolve')]+self.effects()

    def sweep(self, *, decline=False, partial=False, split=False):
        from recipes import step
        # Select the already-current caster's legal main phase, retaining history.
        run = [step('turn', player=self.controller, value=0)]+self.spell('Planar Cleansing', 'sweep', caster=self.controller)
        keys = [key for key, e in self.entities.items() if e['alive'] and 'Land' not in self.oracle[e['name']]['type_line'].partition(' \u2014 ')[0].split()]
        self.death_cut = len(self.actions)+len(run)
        run += self.intent(keys, decline=decline, partial=partial, split=split)
        for key in keys:
            self.leave(key)
        return run+[step('resolve')]+self.effects()

    def blink_source(self):
        from recipes import step
        run = self.spell('Flicker', 'blink', ('source',), caster=self.controller)+[step('resolve')]
        self.source.update(ref='source_new', player=self.source['owner'], generation=1, damage=0, tapped=0)
        return run+[step('bind_permanent', ref='source_new', player=self.source['player'], value=self.registry[self.name]['csvid'])]+self.effects()

    def remove_without_death(self, key, exile=False, spell_key='negative'):
        from recipes import step
        entity = self.entities[key]
        creature = 'Creature' in self.oracle[entity['name']]['type_line']
        spell = ('Swords to Plowshares' if creature else 'Return to Dust') if exile else 'Boomerang'
        run = self.spell(spell, spell_key, (key,))
        if exile and creature:
            self.life[entity['player']] += int(self.oracle[entity['name']]['power'])
        self.leave(key, 'exile' if exile else 'hand')
        return run+[step('resolve')]+self.effects()

    def receipts(self):
        return super().receipts()+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import cost_from_oracle, step
    validate_witnesses(oracle, registry)
    data = pins()
    if name not in data['sources'] or data['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved death-player source')
    if not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('unsupported death-player selection')
    results = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1); player = int(player)
        w = World(name, player, model, registry, oracle, bare=mode == 'bare_zones')
        w.presentation = data['presentation'][name]
        seeded = mode == 'source_foreign_owner'
        w.add(name, 'source', player, 'battlefield' if seeded else 'hand', foreign=seeded)
        kind = model['predicate']['kind']
        witness = 'Goblin Piker' if kind == 'goblin_permanent' else 'Memnite' if kind in ('artifact', 'artifact_or_enchantment') else 'Grizzly Bears'
        who = 1-player if mode in ('opponent_death', 'lent_death') else player
        token = mode == 'token_death'
        if mode in ('artifact_death', 'enchantment_death', 'land_death'):
            witness = {'artifact_death': 'Sol Ring', 'enchantment_death': 'Exploration', 'land_death': 'Forest'}[mode]
        elif mode == 'nonmatching':
            witness = 'Sol Ring' if kind == 'creature' else 'Grizzly Bears' if kind != 'permanent' else 'Memnite'
            token = kind == 'permanent'
        elif mode == 'kindred_goblin':
            witness = 'Boggart Shenanigans'
        elif mode == 'changeling':
            witness = 'Ghostly Changeling'
        w.add(witness, 'victim', who, foreign=mode in ('stolen_death', 'lent_death'), token=token)
        if mode in ('two_deaths', 'split_targets', 'partial_accept'):
            w.add(witness, 'second_victim', who)
        if mode in ('planeswalker', 'own_planeswalker', 'split_targets', 'mandatory_policy', 'decline', 'partial_accept'):
            actor = player if mode == 'own_planeswalker' else 1-player
            walker = w.add('Chandra, Pyrogenius', 'walker', actor)
            walker['loyalty'] = int(oracle['Chandra, Pyrogenius']['loyalty'])
            w.pre += [step('assert_counter', ref='walker', counter_kind='loyalty', value=walker['loyalty'])]
            if mode in ('planeswalker', 'own_planeswalker'):
                w.target = 'walker'
        if mode == 'target_self_player':
            w.target = player
        if mode == 'player_shroud':
            w.add('Ivory Mask', 'player_shield', 1-player)
            w.target = player
        ready_cut = pending_cut = None
        a = w.actions
        if not seeded:
            pool = list(model['mana'])
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'wrong_color':
                unused = next(c for c in range(1, 6) if not pool[c])
                pool = [pool[0], 0, 0, 0, 0, 0]; pool[unused] = sum(model['mana'][1:])
            elif mode == 'generic_colored_payment':
                pool = [0, sum(pool), 0, 0, 0, 0]
            a += w.fund(player, pool); ready_cut = len(a)
            if mode in ('insufficient_mana', 'wrong_color'):
                a += [step('cast', ref='source', value=0)]
            else:
                a += w.cast_source(); pending_cut = len(a)
                if mode == 'countered':
                    w.add('Counterspell', 'counter', 1-player, 'hand')
                    a += w.fund(1-player, cost_from_oracle(oracle['Counterspell']))
                    w.mana[1-player] = [0]*6
                    w.zones['hand'][1-player].remove('Counterspell'); w.zones['graveyard'][1-player].append('Counterspell')
                    w.source['alive'] = False; w.zones['graveyard'][player].append(name)
                    a += [step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                          step('assert_stack', value=2), step('resolve_response')]+w.effects()
                else:
                    a += [step('resolve')]+w.effects()
        if mode not in ('cast_only', 'payment', 'generic_colored_payment', 'insufficient_mana', 'wrong_color', 'countered'):
            if mode in ('victim_bounced', 'victim_exiled'):
                a += w.remove_without_death('victim', exile=mode == 'victim_exiled')
            elif mode == 'source_only':
                a += w.kill('source')
            elif mode == 'source_simultaneous':
                a += w.sweep()
            else:
                if mode == 'source_removed':
                    a += w.kill('source', spell_key='source_removal')
                elif mode in ('source_bounced', 'source_exiled'):
                    a += w.remove_without_death('source', exile=mode == 'source_exiled')
                elif mode == 'source_blinked':
                    a += w.blink_source()
                elif mode == 'prevented':
                    recipient = who if model['recipient'] == 'dead_controller' else w.entities['victim']['owner'] if model['recipient'] == 'graveyard_owner' else player if model['recipient'] == 'you' else 1-player
                    a += w.spell('Safe Passage', 'prevention', caster=recipient)+[step('resolve')]
                    w.prevented.add(recipient)
                if mode in ('two_deaths', 'split_targets', 'partial_accept'):
                    # Wrath preserves a real target walker while killing Creature witnesses.
                    if model['source_creature']:
                        a += w.spell('Indestructibility', 'watcher_shield', ('source',), caster=player)+[step('resolve')]
                        w.entities['watcher_shield']['attached'] = 'source'
                        w.source['indestructible'] = True
                    a += [step('turn', player=player, value=0)]+w.spell('Wrath of God', 'creature_sweep', caster=player)
                    keys = [k for k, e in w.entities.items() if e['alive'] and not e.get('indestructible') and 'Creature' in oracle[e['name']]['type_line']]
                    a += w.intent(keys, partial=mode == 'partial_accept', split=mode == 'split_targets')
                    for key in keys:
                        w.leave(key)
                    a += [step('resolve')]+w.effects()
                elif mode == 'combat_death':
                    w.add('Hill Giant', 'blocker', 1-who)
                    a += w.age(own=who == player)
                    a += [step('begin_attackers'), step('attack', ref='victim', value=1), step('finish_attackers'),
                          step('begin_blockers'), step('block', ref='blocker', other='victim', value=1), step('advance_combat')]
                    a += w.intent(['victim'])
                    w.entities['blocker']['damage'] = int(oracle[witness]['power'])
                    w.leave('victim')
                    a += [step('combat_damage'), step('resolve')]+w.effects()+[step('end_combat')]
                else:
                    a += w.kill(spell={'lethal_death': 'Lightning Bolt', 'zero_toughness': 'Disfigure'}.get(mode),
                                decline=mode == 'decline' or mode == 'mandatory_policy' and model['optional_effect'],
                                policy=mode == 'mandatory_policy')
                    if mode == 'repeat':
                        w.add(witness, 'late_victim', who)
                        a += w.kill('late_victim', spell_key='second_death')
                if mode == 'cleanup_once':
                    a += [step('cleanup'), step('refresh')]+w.effects()
        checks = w.receipts()
        sequence = w.pre+a+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': w.death_cut}[mode]
            if cut is None:
                raise ValueError('actual death-player snapshot boundary missing')
            tail = a[cut:]+checks
            sequence = w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        results.append(dict(name=f'DeathPlayerValueRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                            setup=w.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'],
                            scope_gaps=['independent post-death/pre-effect response window', 'multiplayer',
                                        'AI optional decline and arbitrary identical optional legacy order',
                                        'typed optional player fallback; real planeswalker policy controls used'])))
        if token:
            results[-1]['_recipe']['token_copy_fixture'] = dict(source=witness, copied_type_line=oracle[witness]['type_line'],
                                                              copied_mana_cost=oracle[witness].get('mana_cost', ''),
                                                              copied_mana_value=oracle[witness]['cmc'], creation_tested=False)
    return results
