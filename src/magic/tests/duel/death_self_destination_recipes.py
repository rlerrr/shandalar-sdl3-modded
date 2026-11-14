"""Mandatory self-death destinations, real recasts, and surviving death receipts."""
import copy
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from death_counter_extended_recipes import World as ZoneWorld

FAMILY = 'death_self_destination'


def pins():
    return json.loads((Path(__file__).parent/'data/death_self_destination_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or not record.get('type_line', '').startswith('Creature \u2014 ') or
            any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('whole normal numeric self-death creature required')
    lines = record.get('oracle_text', '').split('\n')
    keys = record.get('keywords', [])
    if keys:
        if (len(keys) != 1 or len(lines) != 2 or keys[0] not in ('Flying', 'Trample') or
                lines[0] not in (keys[0], "Flying (This creature can't be blocked except by creatures with flying or reach.)") or
                lines[0].startswith('Flying') != (keys[0] == 'Flying')):
            raise ValueError('one complete reviewed passive plus self-death clause required')
    elif len(lines) != 1:
        raise ValueError('whole self-death Oracle required')
    subject = '(?:this creature|'+re.escape(record['name'])+')'
    match = re.fullmatch('When '+subject+r' dies, (.+)', lines[-1])
    clauses = {"return it to its owner's hand.": 'hand', "put it on top of its owner's library.": 'top',
               "shuffle it into its owner's library.": 'shuffle', 'exile it.': 'exile'}
    if not match or match[1] not in clauses:
        raise ValueError('whole mandatory self-death destination clause required')
    return dict(destination=clauses[match[1]], keywords=list(keys))


def spec(declaration, record):
    from recipes import cost_from_oracle
    expected = dict(schema=10, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(declaration.get('params'), expected)):
        raise ValueError('typed whole-Oracle self-death destination declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record),
                 power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported self-death variants required')
    return model


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered', 'dies', 'lethal_damage',
             'zero_toughness', 'foreign_owner', 'token_death', 'other_dies', 'opponent_dies', 'bounce', 'exile',
             'simultaneous', 'mixed_simultaneous', 'observer_first', 'pumped_death', 'countered_body_death',
             'cleanup_once', 'bare_zones', 'empty_library', 'ready_snapshot', 'pending_snapshot',
             'death_snapshot', 'resolved_snapshot']
    if model['destination'] in ('hand', 'top'):
        modes += ['recast', 'recast_foreign_owner', 'new_object', 'repeat', 'fresh_attack']
    if model['destination'] == 'top':
        modes += ['draw_returned']
    if model['keywords']:
        modes += ['keyword_before']
        if model['destination'] in ('hand', 'top'):
            modes += ['keyword_after']
    if 'Flying' in model['keywords']:
        modes += ['reach_before']
        if model['destination'] in ('hand', 'top'):
            modes += ['reach_after']
    return [f'{mode}_{player}' for player in (0, 1) for mode in modes]


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record, oracle.get(name)) or not exact(registry.get(name), data['registry'][name]):
            raise ValueError('complete implemented self-death witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(source['oracle'], oracle.get(name)) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('card_dummy', 'NULL', '0')):
            raise ValueError('whole implemented self-death source changed: '+name)
        oracle_model(oracle[name])


class World(ZoneWorld):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.current_player = self.controller
        self.return_number = 0
        self.death_cut = None

    def add(self, *args, **kwargs):
        entity = super().add(*args, **kwargs)
        entity['sick'] = 0
        return entity

    def stats(self, key='source'):
        from recipes import observe
        result = super().stats(key)
        if key == 'source' and self.source['alive']:
            result.append(observe(self.source['ref'], 'sick', self.source['sick']))
        return result

    def effects(self):
        from recipes import step
        return self.stats()+self.stats('observer')+[step('assert_unused_choices', value=0)]+self.mana_checks()

    def depart(self, key='source'):
        """Expected actual death first, followed by the printed mandatory trigger."""
        entity = self.entities[key]
        mob = self.entities['observer']
        if mob['alive'] and key != 'observer' and entity['player'] == mob['player']:
            mob['plus'] += 1
        self.leave(key)
        if key != 'source':
            return
        owner, name = entity['owner'], entity['name']
        destination = self.model['destination']
        # CR111.7/111.8: token dies, then ceases; no durable destination card.
        # CR701.24c: the specific-object shuffle still occurs if it is absent.
        if destination == 'shuffle':
            self.shuffles[owner] += 1
        if entity['token']:
            return
        self.zones['graveyard'][owner].remove(name)
        if destination == 'top':
            self.zones['library'][owner].insert(0, name)
        else:
            self.zones[{'hand': 'hand', 'shuffle': 'library', 'exile': 'exile'}[destination]][owner].append(name)

    def death(self, spell='Murder', key='death_spell'):
        from recipes import step
        run = self.spell(spell, key, ('source',))
        self.death_cut = len(self.actions)+len(run)
        self.depart()
        return run+[step('resolve')]+self.effects()

    def next_main(self, player, force_new_turn=False):
        from recipes import step
        if player == self.current_player and not force_new_turn:
            return []
        turns = 2 if player == self.current_player else 1
        self.current_player = player
        for entity in self.entities.values():
            entity.update(bp=0, bt=0, damage=0, sick=0)
            if entity['alive'] and entity['player'] == player:
                entity['tapped'] = 0
        self.mana = [[0]*6, [0]*6]
        return [step('advance_turn') for _ in range(turns)]+[step('untap_step', player=player),
                    step('turn', player=player, value=0)]+self.effects()

    def cast_source(self, ready=True):
        from recipes import observe, step
        source = self.source
        player, name = source['player'], source['name']
        self.zones['hand'][player].remove(name)
        self.mana[player] = [a-b for a, b in zip(self.mana[player], self.model['mana'])]
        source.update(alive=True, sick=1)
        run = [step('cast', ref=source['ref'], value=1), step('assert_stack', value=1), observe(source['ref'], 'in_hand', 0)]
        return run+([step('resolve'), step('assert_choices_consumed')]+self.effects() if ready else [])

    def draw_returned(self):
        from recipes import step
        owner = self.source['owner']
        run = self.spell('Reach Through Mists', 'draw_returned', caster=owner)+[step('resolve'), step('assert_choices_consumed')]
        assert self.zones['library'][owner][0] == self.name
        self.zones['library'][owner].pop(0)
        self.zones['hand'][owner].append(self.name)
        return run+self.effects()

    def recast(self):
        from recipes import step
        source = self.source
        owner = source['owner']
        run = self.next_main(owner)
        if self.model['destination'] == 'top':
            run += self.draw_returned()
        self.return_number += 1
        source.update(ref=f'returned_{self.return_number}', player=owner, generation=source['generation']+1,
                      plus=0, minus=0, bp=0, bt=0, damage=0, tapped=0, sick=0)
        # Fatal ordinary bind assertion stops dependent casts if the real
        # destination is missing; never force a missing returned card into play.
        run += [step('bind_hand', ref=source['ref'], player=owner, value=self.registry[self.name]['csvid'])]
        run += self.fund(owner, self.model['mana'])+self.cast_source()
        return run

    def combat(self, reach=False):
        from recipes import step
        source = self.source
        player = source['player']
        run = self.next_main(player, force_new_turn=True)
        flying = 'Flying' in self.model['keywords']
        blocker = 'Giant Spider' if reach else 'Grizzly Bears' if flying else 'Eager Cadet'
        self.add(blocker, 'blocker', 1-player)
        blocked = reach or not flying
        power = self.model['power']+source['plus']+source['bp']
        if blocked and power < int(self.oracle[blocker]['toughness']):
            run += self.spell('Giant Growth', 'combat_growth', ('source',), caster=player)+[step('resolve')]
            source.update(bp=3, bt=3)
            power += 3
        source['tapped'] = 1
        run += [step('begin_attackers'), step('attack', ref=source['ref'], value=1), step('finish_attackers'),
                step('begin_blockers'), step('block', ref='blocker', other=source['ref'], value=int(blocked)),
                step('advance_combat'), step('combat_damage'), step('resolve')]
        if blocked:
            if power < int(self.oracle[blocker]['toughness']):
                raise ValueError('real combat fixture must kill its blocker')
            source['damage'] = int(self.oracle[blocker]['power'])
            if 'Trample' in self.model['keywords']:
                self.life[1-player] -= power-int(self.oracle[blocker]['toughness'])
            self.depart('blocker')
        else:
            self.life[1-player] -= power
        if self.model['toughness']+source['plus']+source['bt'] <= source['damage']:
            raise ValueError('real keyword combat must survive before controlled source death')
        return run+self.effects()+[step('require_card', ref=source['ref'], property='in_play', value=1), step('end_combat')]

    def receipts(self):
        return super().receipts()+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import cost_from_oracle, observe, step
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved self-death source')
    if not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unsupported self-death variant')
    results = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        player = int(player)
        w = World(name, player, model, registry, oracle, bare=mode == 'bare_zones')
        if mode == 'empty_library':
            w.zones['library'] = [[], []]
            w.setup = [c for c in w.setup if c['zone'] != 'library']
        if mode == 'observer_first':
            w.add('Unruly Mob', 'observer', player)
        seeded = mode in ('foreign_owner', 'recast_foreign_owner', 'token_death')
        w.add(name, 'source', player, 'battlefield' if seeded else 'hand',
              foreign=mode in ('foreign_owner', 'recast_foreign_owner'), token=mode == 'token_death')
        if mode != 'observer_first':
            w.add('Unruly Mob', 'observer', player)
        if mode in ('other_dies', 'opponent_dies', 'simultaneous', 'mixed_simultaneous'):
            w.add('Eager Cadet', 'other', 1-player if mode == 'opponent_dies' else player)
        if mode == 'mixed_simultaneous':
            w.add('Eager Cadet', 'enemy', 1-player)
        a = w.actions
        ready_cut = pending_cut = None
        if seeded:
            a += w.effects()
        else:
            pool = list(model['mana'])
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'wrong_color':
                color = next(i for i in range(1, 6) if not pool[i])
                pool = [pool[0], 0, 0, 0, 0, 0]
                pool[color] = sum(model['mana'][1:])
            a += w.fund(player, pool)
            ready_cut = len(a)
            if mode in ('insufficient_mana', 'wrong_color'):
                a += [step('cast', ref='source', value=0)]
            else:
                a += w.cast_source(ready=False)
                pending_cut = len(a)
                if mode == 'countered':
                    w.add('Counterspell', 'counter', 1-player, 'hand')
                    a += w.fund(1-player, cost_from_oracle(oracle['Counterspell']))
                    w.mana[1-player] = [0]*6
                    w.zones['hand'][1-player].remove('Counterspell')
                    w.zones['graveyard'][1-player].append('Counterspell')
                    w.source['alive'] = False
                    w.zones['graveyard'][player].append(name)
                    a += [step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                          step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
                else:
                    a += [step('resolve'), step('assert_choices_consumed')]+w.effects()
        if mode not in ('cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered'):
            if mode in ('other_dies', 'opponent_dies'):
                a += w.spell('Murder', 'other_death', ('other',))+[step('resolve')]
                w.depart('other')
                a += w.effects()
            elif mode in ('bounce', 'exile'):
                power = model['power']+w.source['plus']+w.source['bp']
                a += w.spell('Unsummon' if mode == 'bounce' else 'Swords to Plowshares', 'negative', ('source',))+[step('resolve')]
                if mode == 'exile':
                    w.life[player] += power
                w.leave('source', 'hand' if mode == 'bounce' else 'exile')
                a += w.effects()
            elif mode in ('simultaneous', 'mixed_simultaneous'):
                a += w.spell('Withstand Death', 'protect_observer', ('observer',), caster=player)+[step('resolve')]
                a += w.spell('Wrath of God', 'sweep', caster=player)+[step('resolve')]
                w.depart(); w.depart('other')
                if mode == 'mixed_simultaneous':
                    w.depart('enemy')
                a += w.effects()
            else:
                if mode in ('pumped_death', 'new_object'):
                    a += w.spell('Giant Growth', 'body_growth', ('source',), caster=player)+[step('resolve')]
                    w.source.update(bp=3, bt=3)
                    a += w.effects()
                if mode in ('countered_body_death', 'new_object'):
                    a += w.spell('Dragonscale Boon', 'body_counters', ('source',), caster=player)+[step('resolve')]
                    w.source['plus'] = 2
                    a += w.effects()
                if mode == 'new_object':
                    a += w.spell('Shock', 'body_damage', ('source',))+[step('resolve')]
                    w.source['damage'] = 2
                    a += w.effects()
                if mode in ('keyword_before', 'reach_before'):
                    a += w.combat(reach=mode.startswith('reach'))
                a += w.death({'lethal_damage': 'Lightning Blast', 'zero_toughness': 'Grasp of Darkness'}.get(mode, 'Murder'))
                if mode == 'draw_returned':
                    a += w.draw_returned()
                elif mode in ('recast', 'recast_foreign_owner', 'new_object', 'repeat', 'fresh_attack', 'keyword_after', 'reach_after'):
                    a += w.recast()
                    if mode in ('keyword_after', 'reach_after'):
                        a += w.combat(reach=mode.startswith('reach'))
                        a += w.death(key='second_death')
                    elif mode == 'repeat':
                        a += w.death(key='second_death')
                    elif mode == 'fresh_attack':
                        a += [step('begin_attackers'), step('attack', ref=w.source['ref'], value=0),
                              step('finish_attackers'), step('begin_blockers'), step('advance_combat'),
                              step('combat_damage'), step('end_combat')]+w.effects()
                elif mode == 'cleanup_once':
                    a += [step('cleanup'), step('refresh')]+w.effects()
        checks = w.receipts()
        sequence = w.pre+a+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': w.death_cut}[mode]
            if cut is None:
                raise ValueError('missing actual self-death snapshot boundary')
            tail = a[cut:]+checks
            sequence = w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        results.append(dict(name=f'DeathSelfDestinationRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                            setup=w.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'],
                            scope_gaps=['independent after-death/before-trigger response window', 'multiplayer',
                                        'recasting a shuffled/exiled source without a separate real retrieval effect'])))
    return results
