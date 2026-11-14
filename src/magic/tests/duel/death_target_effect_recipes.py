"""Complete targeted self-dies clauses, actual callbacks, independent receipts."""
import copy
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from death_counter_extended_recipes import World as ZoneWorld

FAMILY = 'death_target_effect'


def pins():
    return json.loads((Path(__file__).parent/'data/death_target_effect_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    types = record.get('type_line', '').partition(' \u2014 ')[0].split() if isinstance(record, dict) else []
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or 'Creature' not in types or
            any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('complete normal numeric targeted-death creature required')
    lines, keys = record.get('oracle_text', '').split('\n'), record.get('keywords', [])
    if keys:
        if keys != ['Flying'] or len(lines) != 2 or lines[0] not in ('Flying', "Flying (This creature can't be blocked except by creatures with flying or reach.)"):
            raise ValueError('whole reviewed Flying plus targeted death clause required')
    elif len(lines) != 1:
        raise ValueError('whole single targeted self-death clause required')
    subject = '(?:this creature|'+re.escape(record['name'])+')'
    match = re.fullmatch('When '+subject+r' dies, (.+)', lines[-1])
    if not match:
        raise ValueError('whole targeted self-death clause required')
    clause = match[1]
    model = dict(effect=None, amount=1, counter_kind=None, optional_effect=False, mandatory_target=True,
                 target=dict(kind='creature', controller='any', max_mana_value=None),
                 human_amount=None, witch_amount=None, keywords=list(keys),
                 colored_cost=bool(re.search(r'\{[BUGRW]\}', record.get('mana_cost', ''))))
    counter = re.fullmatch(r'(you may )?put a (\+1/\+1|-1/-1) counter on target creature( you control)?\.', clause)
    removal = re.fullmatch(r'(you may )?(destroy|exile) target (land|permanent|artifact or enchantment|noncreature permanent|creature with mana value ([1-9][0-9]*) or less)\.', clause)
    if counter:
        model.update(effect='counter', counter_kind='plus1' if counter[2] == '+1/+1' else 'minus1', optional_effect=bool(counter[1]))
        model['target']['controller'] = 'you' if counter[3] else 'any'
    elif clause == 'put a +1/+1 counter on target creature you control. If that creature is a Human, put two +1/+1 counters on it instead.':
        model.update(effect='counter', counter_kind='plus1', human_amount=2)
        model['target']['controller'] = 'you'
    elif clause == 'target creature an opponent controls gets -1/-1 until end of turn. That creature gets -4/-4 instead if you control a creature named Bogbrew Witch.':
        model.update(effect='temporary_stats', witch_amount=4)
        model['target']['controller'] = 'opponent'
    elif clause in ('target creature gets -1/-1 until end of turn.',
                    'target creature an opponent controls gets -1/-1 until end of turn.',
                    'you may have target creature get -2/-2 until end of turn.'):
        model.update(effect='temporary_stats', amount=2 if clause.startswith('you may') else 1,
                     optional_effect=clause.startswith('you may'))
        if 'an opponent controls' in clause:
            model['target']['controller'] = 'opponent'
    elif removal:
        model.update(effect=removal[2], optional_effect=bool(removal[1]))
        model['target']['kind'] = 'creature' if removal[4] else removal[3].replace(' ', '_')
        model['target']['max_mana_value'] = int(removal[4]) if removal[4] else None
    else:
        raise ValueError('unreviewed full targeted-death effect')
    return model


def spec(declaration, record):
    from recipes import cost_from_oracle
    expected = dict(schema=11, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(declaration.get('params'), expected)):
        raise ValueError('typed complete targeted-death declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record),
                 power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported targeted-death variants required')
    return model


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'countered', 'effect',
             'source_foreign_owner', 'source_token', 'target_foreign_owner', 'target_token',
             'other_dies', 'source_bounced', 'source_exiled', 'source_blinked', 'empty_targets',
             'lethal_source', 'zero_source', 'cleanup', 'two_sources', 'bare_zones',
             'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
    modes += ['wrong_color' if model['colored_cost'] else 'generic_colored_payment']
    if model['target']['kind'] != 'permanent':
        modes += ['invalid_only']
    if model['target']['controller'] in ('any', 'you'):
        modes += ['own_target']
    if model['target']['controller'] in ('any', 'opponent'):
        modes += ['opponent_target']
    if model['target']['kind'] in ('creature', 'permanent', 'artifact_or_enchantment'):
        modes += ['target_indestructible', 'target_simultaneous']
    if model['target']['kind'] in ('creature', 'permanent'):
        modes += ['shroud_only']
    if model['effect'] in ('counter', 'temporary_stats'):
        modes += ['lethal_target' if model['effect'] == 'temporary_stats' or model['counter_kind'] == 'minus1' else 'small_target',
                  'target_blinked', 'target_event']
    if model['effect'] == 'counter':
        modes += ['counter_cancel']
    if model['human_amount']:
        modes += ['human_target', 'changeling_target', 'nonhuman_target']
    if model['witch_amount']:
        modes += ['witch', 'opponent_witch', 'witch_removed', 'witch_bounced', 'witch_foreign']
    if model['target']['max_mana_value'] is not None:
        modes += ['mv_zero', 'mv_boundary', 'mv_above']
    if 'Flying' in model['keywords']:
        modes += ['flying_combat', 'reach_combat']
    result = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    if model['optional_effect']:
        result += ['decline_0', 'decline_target_event_0']
    else:
        result += ['mandatory_policy_0']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record, oracle.get(name)) or not exact(data['registry'][name], registry.get(name)):
            raise ValueError('whole implemented target-death witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(source['oracle'], oracle.get(name)) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('NULL', 'card_dummy', '0')):
            raise ValueError('whole implemented targeted-death source changed: '+name)
        oracle_model(oracle[name])


class World(ZoneWorld):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.forced_targets = 0
        self.death_cut = None

    def all_stats(self):
        return [s for key, entity in self.entities.items() if entity['alive'] for s in self.stats(key)]

    def effects(self):
        from recipes import step
        return self.all_stats()+[step('assert_unused_choices', value=0),
                                 step('assert_forced_target_choices', value=self.forced_targets)]+self.mana_checks()

    def eligible(self, key):
        entity = self.entities[key]
        if not entity['alive']:
            return False
        record, target = self.oracle[entity['name']], self.model['target']
        types = set(record['type_line'].partition(' \u2014 ')[0].split())
        if not {'Creature', 'Artifact', 'Enchantment', 'Land', 'Planeswalker', 'Battle'} & types:
            return False
        kind = target['kind']
        if not {'creature': 'Creature' in types, 'land': 'Land' in types,
                'artifact_or_enchantment': bool(types & {'Artifact', 'Enchantment'}),
                'noncreature_permanent': 'Creature' not in types, 'permanent': True}[kind]:
            return False
        if target['controller'] == 'you' and entity['player'] != self.controller or target['controller'] == 'opponent' and entity['player'] == self.controller:
            return False
        if target['max_mana_value'] is not None and record['cmc'] > target['max_mana_value']:
            return False
        return 'Shroud' not in record.get('keywords', [])

    def target_plan(self, target, source='source', decline=False, policy=False):
        from recipes import step
        if not self.eligible(target):
            return []
        run = []
        if self.controller == 0 and pins()['presentation'][self.name]['outer_priority_human']:
            # Capture CSVID while the actual dying source is still alive and
            # Murder is pending; the deferred plan selects its offered legacy.
            run += [step('choose_trigger', ref=self.entities[source]['ref'], player=self.controller)]
        if decline or policy:
            run += [step('choose_optional_target', ref=self.entities[target]['ref'], player=self.controller)]
            # Every clause has a mandatory target even if its effect says may.
            # If the engine cancels targeting, expose policy+effect receipts as
            # ordinary assertions; never force an illegal cancellation input.
            self.forced_targets += 1
        else:
            run += [step('choose_target', ref=self.entities[target]['ref'], player=self.controller)]
        return run

    def expected_effect(self, target, decline=False):
        if not self.eligible(target):
            return
        entity = self.entities[target]
        record = self.oracle[entity['name']]
        if entity['name'] == 'Task Force':
            # Real becomes-target trigger. It remains mandatory when the
            # targeted death ability's optional effect is declined (CR603.5).
            entity['bt'] += 3
        if decline:
            return
        amount = self.model['amount']
        if self.model['human_amount'] and ('Human' in record['type_line'].partition(' \u2014 ')[2].split() or 'Changeling' in record.get('keywords', [])):
            amount = self.model['human_amount']
        if self.model['witch_amount'] and any(e['alive'] and e['name'] == 'Bogbrew Witch' and e['player'] == self.controller for e in self.entities.values()):
            amount = self.model['witch_amount']
        effect = self.model['effect']
        if effect == 'counter':
            entity['plus' if self.model['counter_kind'] == 'plus1' else 'minus'] += amount
            cancelled = min(entity['plus'], entity['minus'])
            entity['plus'] -= cancelled; entity['minus'] -= cancelled
        elif effect == 'temporary_stats':
            entity['bp'] -= amount; entity['bt'] -= amount
        elif effect == 'exile':
            self.leave(target, 'exile')
        elif 'Indestructible' not in record.get('keywords', []):
            self.leave(target)
        if entity['alive'] and 'Creature' in record['type_line'] and int(record['toughness'])+entity['plus']-entity['minus']+entity['bt'] <= 0:
            self.leave(target)

    def death(self, source='source', target='recipient', spell='Murder', key='death_spell', decline=False, policy=False):
        from recipes import step
        run = self.spell(spell, key, (source,))
        self.death_cut = len(self.actions)+len(run)
        run += self.target_plan(target, source, decline, policy)
        self.leave(source)
        self.expected_effect(target, decline)
        return run+[step('resolve')]+self.effects()

    def cast_source(self, key='source'):
        from recipes import observe, step
        entity = self.entities[key]
        player = entity['player']
        payment = ([step('choose_mana', player=player, color=color)
                    for color in range(1, 6) for _ in range(self.mana[player][color])]
                   if player == 0 and not self.model['colored_cost'] else [])
        self.zones['hand'][player].remove(entity['name'])
        entity['alive'] = True
        self.mana[player] = ([0]*6 if not self.model['colored_cost'] else
                             [a-b for a, b in zip(self.mana[player], self.model['mana'])])
        return payment+[step('cast', ref=entity['ref'], value=1), step('assert_stack', value=1), observe(entity['ref'], 'in_hand', 0)]

    def cleanup_receipts(self):
        from recipes import step
        for entity in self.entities.values():
            entity.update(bp=0, bt=0, damage=0)
        return [step('cleanup'), step('refresh')]+self.effects()

    def flying_combat(self, reach=False):
        from recipes import step
        run = self.age()
        self.add('Giant Spider' if reach else 'Grizzly Bears', 'blocker', 1-self.controller)
        power = self.model['power']
        if reach and power < int(self.oracle['Giant Spider']['toughness']):
            run += self.spell('Giant Growth', 'combat_growth', ('source',), caster=self.controller)+[step('resolve')]
            self.source.update(bp=3, bt=3)
            power += 3
        self.source['tapped'] = 1
        run += [step('begin_attackers'), step('attack', ref='source', value=1), step('finish_attackers'),
                step('begin_blockers'), step('block', ref='blocker', other='source', value=int(reach)),
                step('advance_combat'), step('combat_damage'), step('resolve')]
        if reach:
            self.source['damage'] = 2
            self.leave('blocker')
        else:
            self.life[1-self.controller] -= power
        return run+self.effects()+[step('require_card', ref='source', property='in_play', value=1), step('end_combat')]

    def receipts(self):
        return super().receipts()+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import cost_from_oracle, step
    validate_witnesses(oracle, registry)
    data = pins()
    if name not in data['sources'] or data['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved targeted-death source')
    if not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unsupported targeted-death selection')
    results = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1); player = int(player)
        w = World(name, player, model, registry, oracle, bare=mode == 'bare_zones')
        seeded = mode in ('source_foreign_owner', 'source_token')
        w.add(name, 'source', player, 'battlefield' if seeded else 'hand', foreign=mode == 'source_foreign_owner', token=mode == 'source_token')
        target = model['target']
        actor = player if target['controller'] == 'you' else 1-player
        witness = 'Grizzly Bears' if target['kind'] == 'creature' else 'Sol Ring' if target['kind'] == 'permanent' else 'Exploration' if target['kind'] == 'artifact_or_enchantment' else 'Forest'
        if target['max_mana_value'] is not None:
            witness = 'Centaur Courser'
        if mode == 'own_target':
            actor = player
        elif mode == 'opponent_target':
            actor = 1-player
        if mode in ('human_target', 'lethal_target', 'small_target'):
            witness = 'Eager Cadet'
        elif mode == 'changeling_target':
            witness = 'Woodland Changeling'
        elif mode == 'nonhuman_target':
            witness = 'Grizzly Bears'
        elif mode in ('target_event', 'decline_target_event'):
            witness = 'Task Force'
        elif mode in ('target_token', 'mv_zero'):
            witness = ('Forest' if target['kind'] == 'land' else 'Sol Ring' if target['kind'] == 'noncreature_permanent'
                       else 'Memnite')
        elif mode == 'mv_above':
            witness = 'Hill Giant'
        elif mode == 'target_indestructible':
            witness = 'Darksteel Myr'
        elif mode == 'shroud_only':
            witness = 'Sphinx of Jwar Isle'
        elif mode == 'invalid_only':
            if target['controller'] in ('you', 'opponent'):
                actor = 1-player if target['controller'] == 'you' else player
            elif target['max_mana_value'] is not None:
                witness = 'Hill Giant'
            else:
                witness = ('Sol Ring' if target['kind'] == 'creature' else 'Memnite' if target['kind'] == 'noncreature_permanent'
                           else 'Grizzly Bears')
        elif mode.startswith('witch') or mode == 'opponent_witch':
            witness = 'Scaled Wurm'
        elif mode == 'target_simultaneous':
            witness = 'Ornithopter' if target['kind'] == 'artifact_or_enchantment' else 'Grizzly Bears'
        if model['effect'] == 'temporary_stats' and model['amount'] == 2 and mode in ('cleanup', 'target_blinked'):
            witness = 'Centaur Courser'
        if mode != 'empty_targets':
            w.add(witness, 'recipient', actor, foreign=mode == 'target_foreign_owner', token=mode == 'target_token')
        else:
            # A named absent ref is unnecessary; no plan/effect is requested.
            w.entities['recipient'] = dict(name=witness, ref='absent', alive=False)
        if mode == 'two_sources':
            w.add(name, 'source_second', player, 'hand')
            w.add(witness, 'recipient_second', actor)
        if mode in ('witch', 'opponent_witch', 'witch_removed', 'witch_bounced', 'witch_foreign'):
            w.add('Bogbrew Witch', 'witch', 1-player if mode == 'opponent_witch' else player, foreign=mode == 'witch_foreign')
        a = w.actions; ready_cut = pending_cut = None
        if seeded:
            a += w.effects()
        else:
            pool = list(model['mana'])
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'generic_colored_payment':
                pool = [0, sum(pool), 0, 0, 0, 0]
            elif mode == 'wrong_color':
                color = next(c for c in range(1, 6) if not pool[c])
                pool = [pool[0], 0, 0, 0, 0, 0]; pool[color] = sum(model['mana'][1:])
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
                          step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
                else:
                    a += [step('resolve'), step('assert_choices_consumed')]+w.effects()
        if mode not in ('cast_only', 'payment', 'generic_colored_payment', 'insufficient_mana', 'wrong_color', 'countered'):
            if mode == 'other_dies':
                w.add('Eager Cadet', 'unrelated', player)
                a += w.spell('Murder', 'other_death', ('unrelated',))+[step('resolve')]
                w.leave('unrelated'); a += w.effects()
            elif mode in ('source_bounced', 'source_exiled'):
                a += w.spell('Unsummon' if mode == 'source_bounced' else 'Swords to Plowshares', 'negative', ('source',))+[step('resolve')]
                if mode == 'source_exiled':
                    w.life[player] += model['power']
                w.leave('source', 'hand' if mode == 'source_bounced' else 'exile'); a += w.effects()
            elif mode == 'source_blinked':
                a += w.blink('source')+w.effects()
            elif mode == 'target_simultaneous':
                a += w.spell('Wrath of God', 'sweep', caster=player)
                w.leave('source'); w.leave('recipient')
                a += [step('resolve')]+w.effects()
            else:
                if mode in ('witch_removed', 'witch_bounced'):
                    a += w.spell('Murder' if mode == 'witch_removed' else 'Unsummon', 'witch_removal', ('witch',))+[step('resolve')]
                    w.leave('witch', 'graveyard' if mode == 'witch_removed' else 'hand'); a += w.effects()
                if mode in ('flying_combat', 'reach_combat'):
                    a += w.flying_combat(mode == 'reach_combat')
                a += w.death(spell={'lethal_source': 'Lightning Blast', 'zero_source': 'Grasp of Darkness'}.get(mode, 'Murder'),
                             decline=mode.startswith('decline'), policy=mode == 'mandatory_policy')
                if mode == 'cleanup':
                    a += w.cleanup_receipts()
                elif mode == 'target_blinked' and w.entities['recipient']['alive']:
                    a += w.blink('recipient')+w.effects()
                elif mode == 'counter_cancel' and w.entities['recipient']['alive']:
                    if model['counter_kind'] == 'plus1':
                        a += [step('require_card', ref=w.entities['recipient']['ref'], property='plus1_counters', value=1)]
                        a += w.spell('Virulent Wound', 'cancel_counter', ('recipient',))+[step('resolve')]
                        w.entities['recipient']['plus'] -= 1
                    else:
                        a += [step('require_card', ref=w.entities['recipient']['ref'], property='minus1_counters', value=1)]
                        a += w.spell('Dragonscale Boon', 'cancel_counter', ('recipient',), caster=actor)+[step('resolve')]
                        w.entities['recipient'].update(minus=0, plus=1, tapped=0)
                    a += w.effects()
                elif mode == 'two_sources':
                    a += w.fund(player, model['mana'])+w.cast_source('source_second')+[step('resolve')]+w.effects()
                    a += w.death(source='source_second', target='recipient_second', key='second_death')
        checks = w.receipts()
        sequence = w.pre+a+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': w.death_cut}[mode]
            if cut is None:
                raise ValueError('actual targeted-death snapshot boundary missing')
            tail = a[cut:]+checks
            sequence = w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        results.append(dict(name=f'DeathTargetEffectRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                            setup=w.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'],
                            scope_gaps=['independent post-death/pre-effect response window', 'multiplayer',
                                        'arbitrary identical optional legacy source ordering'])))
        if mode == 'target_token' and target['kind'] in ('land', 'noncreature_permanent'):
            results[-1]['_recipe']['token_copy_fixture'] = dict(source=witness, copied_type_line=oracle[witness]['type_line'],
                                                               copied_mana_cost=oracle[witness].get('mana_cost', ''),
                                                               copied_mana_value=oracle[witness]['cmc'], creation_tested=False)
    return results
