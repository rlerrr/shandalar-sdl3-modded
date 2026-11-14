"""Whole-Oracle death counters composed with traits and actual damage history."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from death_counter_trigger_recipes import exact
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'death_counter_extended'


def pins():
    return json.loads((Path(__file__).parent/'data/death_counter_extended_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or not record.get('type_line', '').startswith('Creature \u2014 ') or
            any(not re.fullmatch(r'[1-9][0-9]*', record.get(k, '')) for k in ('power', 'toughness'))):
        raise ValueError('complete normal numeric death-counter creature required')
    lines = record.get('oracle_text', '').split('\n')
    keywords = record.get('keywords')
    if keywords:
        if len(lines) != 2 or len(keywords) != 1:
            raise ValueError('one complete reviewed keyword plus death clause required')
        keyword = keywords[0]
        presentations = {'Flying': ('Flying', "Flying (This creature can't be blocked except by creatures with flying or reach.)"),
                         'First strike': ('First strike',),
                         'Shroud': ("Shroud (This creature can't be the target of spells or abilities.)",)}
        if keyword not in presentations or lines[0] not in presentations[keyword]:
            raise ValueError('unreviewed printed death-counter keyword')
    elif len(lines) != 1:
        raise ValueError('whole death-counter Oracle required')
    subject = r'(?:this creature|'+re.escape(record['name'])+')'
    match = re.fullmatch(r'Whenever (.+), (you may )?put a \+1/\+1 counter on '+subject+r'\.', lines[-1])
    predicates = {'another creature dies': 'another_creature',
                  'a creature with flying dies': 'flying_creature',
                  'another Human creature dies': 'another_human_creature',
                  'a creature dealt damage by this creature this turn dies': 'source_damaged_this_turn'}
    if not match or match[1] not in predicates:
        raise ValueError('whole reviewed death-counter predicate required')
    return dict(predicate=predicates[match[1]], optional=bool(match[2]), counters=1, keywords=list(keywords))


def spec(declaration, record):
    from recipes import cost_from_oracle
    expected = dict(schema=9, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(declaration.get('params'), expected)):
        raise ValueError('typed whole-Oracle extended death-counter declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record),
                 power=int(record['power']), toughness=int(record['toughness']))
    selected = declaration.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported extended death-counter variants required')
    return model


def variants(model):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered',
             'own_death', 'opponent_death', 'stolen_death', 'lent_death', 'source_foreign_owner',
             'token_death', 'nonmatching', 'noncreature_artifact', 'source_removed', 'source_bounced',
             'source_exiled', 'source_simultaneous', 'repeat', 'cleanup_once', 'counter_cancel',
             'temporary_pump', 'bare_zones', 'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
    if 'Shroud' not in model['keywords']:
        modes += ['source_blinked']
    else:
        modes += ['shroud_probe']
    if 'Flying' in model['keywords'] or 'First strike' in model['keywords']:
        modes += ['keyword_before', 'keyword_after']
    if 'Flying' in model['keywords']:
        modes += ['reach_before', 'reach_after']
    if model['predicate'] == 'flying_creature':
        modes += ['flying_gained', 'flying_lost']
    elif model['predicate'] == 'another_human_creature':
        modes += ['changeling_death']
    elif model['predicate'] == 'source_damaged_this_turn':
        modes += ['prevented_death', 'undamaged_death', 'expired_history', 'source_new_object', 'victim_new_object']
    if not model['optional']:
        modes += ['simultaneous']
    result = [f'{mode}_{player}' for player in (0, 1) for mode in modes]
    if model['optional']:
        result += ['decline_0', 'partial_accept_0']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if name not in registry or not exact(record, oracle.get(name)):
            raise ValueError('complete extended death-counter witness changed: '+name)
        row = registry[name]
        if not exact(row, data['registry'][name]):
            raise ValueError('extended death-counter witness implementation changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(source['oracle'], oracle.get(name)) or row.get('csvid') != source['csvid'] or
                row.get('handler') != source['handler'] or row.get('kind') != 'card' or
                row.get('handler') in ('NULL', 'card_dummy', '0')):
            raise ValueError('implemented extended death-counter source changed: '+name)
        oracle_model(oracle[name])


class World:
    """Oracle-derived independent receipts; no engine state or rules seeding."""
    def __init__(self, name, controller, model, registry, oracle, bare=False):
        from recipes import card, step
        self.name, self.controller, self.model, self.registry, self.oracle = name, controller, model, registry, oracle
        self.setup, self.entities, self.actions = [], {}, []
        self.zones = {z: [list(names) if not bare else [] for _ in (0, 1)]
                      for z, names in [('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE)]}
        self.zones['library'] = [list(LIBRARY), list(LIBRARY)]
        self.life, self.mana, self.shuffles = [20, 20], [[0]*6, [0]*6], [0, 0]
        self.marked = set()
        self.pre = [step('reset_death_history'), step('turn', player=controller, value=0)]
        self.pre += [step('set_mana', player=p, color=c, value=0) for p in (0, 1) for c in range(6)]
        self.pre += [step('assert_shuffle_count', player=p, value=0) for p in (0, 1)]
        for z, players in self.zones.items():
            for p, names in enumerate(players):
                self.setup += [card(n, f'{z}_{p}_{i}', p, z) for i, n in enumerate(names)]

    def add(self, name, key, player, zone='battlefield', foreign=False, token=False):
        from recipes import card
        item = card(name, key, player, zone)
        if zone == 'battlefield':
            item['state'] = ['untapped']+(['opponent_owned'] if foreign else [])+(['token'] if token else [])
        self.setup.append(item)
        self.entities[key] = dict(key=key, ref=key, name=name, player=player, owner=player ^ foreign,
                                  alive=zone == 'battlefield', token=token, plus=0, minus=0,
                                  bp=0, bt=0, damage=0, tapped=0, generation=0,
                                  flying='Flying' in self.oracle[name].get('keywords', []), grounded=False)
        if zone != 'battlefield':
            self.zones[zone][player].append(name)
        return self.entities[key]

    @property
    def source(self):
        return self.entities['source']

    def fund(self, player, pool):
        from recipes import step
        self.mana[player] = list(pool)
        return [step('set_mana', player=player, color=c, value=v) for c, v in enumerate(pool)]

    def stats(self, key='source'):
        from recipes import observe
        e = self.entities[key]
        if not e['alive']:
            return []
        r = self.oracle[e['name']]
        return [observe(e['ref'], p, v) for p, v in (
            ('in_play', 1), ('in_hand', 0), ('csvid', self.registry[e['name']]['csvid']),
            ('plus1_counters', e['plus']), ('minus1_counters', e['minus']),
            ('power', int(r.get('power', 0))+e['plus']-e['minus']+e['bp']),
            ('toughness', int(r.get('toughness', 0))+e['plus']-e['minus']+e['bt']),
            ('damage', e['damage']), ('tapped', e['tapped']))]

    def mana_checks(self):
        from recipes import step
        return [step('assert_mana', player=p, color=c, value=v) for p in (0, 1) for c, v in enumerate(self.mana[p])]

    def effects(self):
        from recipes import step
        return self.stats()+[step('assert_unused_choices', value=0)]+self.mana_checks()

    def spell(self, name, key, targets=(), caster=None, x=None, destination='graveyard'):
        from recipes import cost_from_oracle, step
        p = 1-self.controller if caster is None else caster
        self.add(name, key, p, 'hand')
        record = self.oracle[name]
        if x is None:
            cost = cost_from_oracle(record)
        else:
            fixed = dict(record, mana_cost=record['mana_cost'].replace('{X}', ''))
            cost = cost_from_oracle(fixed)
            cost[0] += x
        run = self.fund(p, cost)
        if x is not None:
            run.append(step('choose_x', player=p, value=x))
        run += [step('choose_target', player=p, ref=self.entities[t]['ref']) for t in targets]
        run += [step('cast', ref=key, value=1), step('assert_stack', value=1)]
        self.zones['hand'][p].remove(name)
        permanent = 'Enchantment' in record['type_line'] or 'Artifact' in record['type_line']
        self.entities[key]['alive'] = permanent
        if not permanent:
            self.zones[destination][p].append(name)
        self.mana[p] = [a-b for a, b in zip(self.mana[p], cost)]
        return run

    def leave(self, key, zone='graveyard'):
        e = self.entities[key]
        e['alive'] = False
        if not e['token']:
            self.zones[zone][e['owner']].append(e['name'])
        for aura in list(self.entities.values()):
            if aura.get('attached') == key and aura['alive']:
                self.leave(aura['key'])

    def qualifies(self, key):
        e = self.entities[key]
        r = self.oracle[e['name']]
        if 'Creature' not in r['type_line']:
            return False
        predicate = self.model['predicate']
        if predicate == 'source_damaged_this_turn':
            return (key, e['generation']) in self.marked
        if predicate == 'flying_creature':
            return e['flying']
        if key == 'source':
            return False
        if predicate == 'another_human_creature':
            return 'Human' in r['type_line'].partition(' \u2014 ')[2].split() or 'Changeling' in r.get('keywords', [])
        return True

    def intent(self, keys, decline=False, partial=False):
        from recipes import step
        count = sum(self.qualifies(k) for k in keys)
        if not self.source['alive'] or not count:
            return []
        accepted = 0 if decline else 1 if partial else count
        self.source['plus'] += accepted
        if self.model['optional'] and self.controller == 0 and not decline:
            if count > 1 and not partial:
                raise ValueError('uncalibrated independent optional all-accept order')
            return [step('choose_trigger_source', ref=self.source['ref'], player=self.controller)]
        return []

    def mark_damage(self, key='victim', prevented=False):
        from recipes import step
        target = self.entities[key]
        own = target['player'] == self.controller
        spell = 'Fall of the Hammer' if own else 'Prey Upon'
        run = self.spell(spell, 'damage_'+str(len(self.setup)), ('source', key), caster=self.controller)
        run += [step('resolve'), step('assert_choices_consumed')]
        amount = 0 if prevented else int(self.oracle[self.name]['power'])+self.source['plus']+self.source['bp']
        if amount:
            self.marked.add((key, target['generation']))
        target['damage'] += amount
        # These deliberately nonlethal0/8 witnesses deal0 back in a fight.
        return run+self.stats(key)+self.effects()

    def kill(self, key='victim', removal='removal', decline=False):
        from recipes import step
        noncreature = 'Creature' not in self.oracle[self.entities[key]['name']]['type_line']
        run = self.spell('Naturalize' if noncreature else 'Murder', removal, (key,))
        run += self.intent([key], decline=decline) if key != 'source' else []
        self.leave(key)
        return run+[step('resolve')]+self.effects()

    def blink(self, key='source', spell_key='blink'):
        from recipes import step
        e = self.entities[key]
        run = self.spell('Cloudshift', spell_key, (key,), caster=e['player'])+[step('resolve'), step('assert_choices_consumed')]
        e.update(ref=key+'_new', player=e['owner'], generation=e['generation']+1, plus=0, minus=0,
                 bp=0, bt=0, damage=0, tapped=0)
        if key == 'source':
            self.marked.clear()
        return run+[step('bind_permanent', ref=e['ref'], player=e['player'], value=self.registry[e['name']]['csvid'])]+self.stats(key)

    def age(self, own=True):
        from recipes import step
        p = self.controller if own else 1-self.controller
        for e in self.entities.values():
            e.update(damage=0, bp=0, bt=0)
            if e['alive'] and e['player'] == p:
                e['tapped'] = 0
            e['flying'] = 'Flying' in self.oracle[e['name']].get('keywords', []) and not e['grounded']
        self.marked.clear()
        self.mana = [[0]*6, [0]*6]
        return [step('advance_turn') for _ in range(2 if own else 1)]+[step('untap_step', player=p), step('turn', player=p, value=0)]+self.effects()

    def receipts(self):
        from recipes import step
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]+self.stats()+self.mana_checks()
        universe = {c['name'] for c in self.setup}
        for p in (0, 1):
            checks += [step('assert_life', player=p, value=self.life[p]), step('assert_poison', player=p, value=0),
                       step('assert_energy', player=p, value=0), step('assert_shuffle_count', player=p, value=self.shuffles[p])]
            for zone, players in self.zones.items():
                names = players[p]
                counts = Counter(names)
                checks += [step('assert_zone', player=p, zone=zone, value=len(names))]
                checks += [step('assert_zone_multiplicity', player=p, zone=zone, index=self.registry[n]['csvid'], value=counts[n]) for n in sorted(universe)]
                if zone == 'library' and not self.shuffles[p]:
                    checks += [step('assert_zone_card', player=p, zone=zone, index=i, value=self.registry[n]['csvid']) for i, n in enumerate(names)]
            alive = [e for e in self.entities.values() if e['alive'] and e['player'] == p]
            checks += [step('assert_zone', player=p, zone='battlefield', value=len(alive))]
            checks += [step('assert_zone_multiplicity', player=p, zone='battlefield', index=self.registry[n]['csvid'],
                            value=sum(e['name'] == n for e in alive)) for n in sorted(universe)]
        return checks


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, observe, cost_from_oracle
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved extended death-counter source')
    if not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unsupported extended death-counter selection')
    results = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        player = int(player)
        w = World(name, player, model, registry, oracle, bare=mode == 'bare_zones')
        seeded = mode == 'source_foreign_owner'
        w.add(name, 'source', player, 'battlefield' if seeded else 'hand', foreign=seeded)
        history = model['predicate'] == 'source_damaged_this_turn'
        shroud = 'Shroud' in model['keywords']
        witness = ('Wall of Stone' if history else 'Ornithopter' if model['predicate'] == 'flying_creature'
                   else 'Eager Cadet' if model['predicate'] == 'another_human_creature' else 'Grizzly Bears')
        who = 1-player if history else player
        if mode in ('own_death', 'stolen_death'):
            who = player
        elif mode in ('opponent_death', 'lent_death'):
            who = 1-player
        if mode == 'nonmatching':
            witness = 'Grizzly Bears' if model['predicate'] in ('flying_creature', 'another_human_creature') else 'Sol Ring'
        elif mode == 'noncreature_artifact':
            witness = 'Sol Ring'
        elif mode == 'flying_gained':
            witness = 'Grizzly Bears'
        elif mode == 'changeling_death':
            witness = 'Woodland Changeling'
        if mode == 'partial_accept':
            witness = 'Ornithopter' if model['predicate'] == 'flying_creature' else 'Eager Cadet'
            who = 1-player if shroud else player
        w.add(witness, 'victim', who, foreign=mode in ('stolen_death', 'lent_death'), token=mode == 'token_death')
        if history and who == player:
            # Fall of the Hammer's AI CAN_CAST still wants an opposing legal
            # candidate; the actual explicit second target may be our corpse.
            w.add('Wall of Stone', 'legal_opponent', 1-player)
        if mode in ('repeat', 'source_blinked', 'simultaneous', 'partial_accept', 'source_simultaneous', 'undamaged_death'):
            w.add(witness, 'second_victim', who)
        ready_cut = pending_cut = death_cut = None
        a = w.actions
        if not seeded:
            pool = list(model['mana'])
            if mode == 'insufficient_mana':
                pool = [0]*6
            elif mode == 'wrong_color':
                unused = next(c for c in range(1, 6) if not pool[c])
                pool = [pool[0], 0, 0, 0, 0, 0]
                pool[unused] = sum(model['mana'][1:])
            a += w.fund(player, pool)
            ready_cut = len(a)
            if mode in ('insufficient_mana', 'wrong_color'):
                a += [step('cast', ref='source', value=0)]
            else:
                w.zones['hand'][player].remove(name)
                w.mana[player] = [0]*6
                a += [step('cast', ref='source', value=1), step('assert_stack', value=1), observe('source', 'in_hand', 0)]
                pending_cut = len(a)
                if mode == 'countered':
                    w.add('Counterspell', 'counter', 1-player, 'hand')
                    a += w.fund(1-player, cost_from_oracle(oracle['Counterspell']))
                    w.mana[1-player] = [0]*6
                    w.zones['hand'][1-player].remove('Counterspell')
                    w.zones['graveyard'][1-player].append('Counterspell')
                    w.zones['graveyard'][player].append(name)
                    a += [step('announce_spell', ref='source'), step('cast_response', ref='counter', value=1),
                          step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
                else:
                    w.source['alive'] = True
                    a += [step('resolve'), step('assert_choices_consumed')]+w.stats()+w.mana_checks()
        else:
            a += w.stats()
        skip = mode in ('cast_only', 'payment', 'insufficient_mana', 'wrong_color', 'countered')

        def prepare(ref='victim'):
            if history and mode not in ('nonmatching', 'noncreature_artifact', 'undamaged_death'):
                return w.mark_damage(ref)
            return []

        def regular_death(ref='victim', removal='removal', decline=False):
            nonlocal death_cut
            a.extend(prepare(ref))
            prefix = w.spell('Naturalize' if 'Creature' not in oracle[w.entities[ref]['name']]['type_line'] else 'Murder', removal, (ref,))
            death_cut = len(a)+len(prefix)
            a.extend(prefix+w.intent([ref], decline=decline)+[step('resolve')])
            w.leave(ref)
            a.extend(w.effects())

        if not skip:
            if mode == 'shroud_probe':
                for p in (0, 1):
                    w.add('Giant Growth', f'probe_{p}', p, 'hand')
                    a += [step('can_target_creature', ref=f'probe_{p}', other='source', value=0),
                          step('can_target_creature', ref=f'probe_{p}', other='victim', value=1)]
                regular_death()
            elif mode in ('source_removed', 'source_bounced', 'source_exiled'):
                a += prepare()
                if shroud:
                    n = {'source_removed': 'Wrath of God', 'source_bounced': 'Evacuation', 'source_exiled': 'Final Judgment'}[mode]
                    a += w.spell(n, 'source_removal', caster=player)+[step('resolve'), step('assert_unused_choices', value=0)]
                    zone = {'source_removed': 'graveyard', 'source_bounced': 'hand', 'source_exiled': 'exile'}[mode]
                    for key in ('source', 'victim'):
                        w.leave(key, zone)
                else:
                    n = {'source_removed': 'Murder', 'source_bounced': 'Unsummon', 'source_exiled': 'Swords to Plowshares'}[mode]
                    a += w.spell(n, 'source_removal', ('source',))+[step('resolve'), step('assert_unused_choices', value=0)]
                    if mode == 'source_exiled':
                        w.life[player] += model['power']+w.source['plus']
                    w.leave('source', {'source_removed': 'graveyard', 'source_bounced': 'hand', 'source_exiled': 'exile'}[mode])
                    a += w.kill()
            elif mode == 'source_simultaneous':
                a += prepare()
                a += w.spell('Wrath of God', 'sweep', caster=player)+[step('resolve'), step('assert_unused_choices', value=0)]
                for key in ('source', 'victim', 'second_victim'):
                    w.leave(key)
            elif mode in ('simultaneous', 'partial_accept'):
                if history:
                    a += w.mark_damage()+w.mark_damage('second_victim')
                    # Only marked victims must die; protect watcher with a real
                    # untargeted indestructible effect before actual Wrath.
                    a += w.spell('Make a Stand', 'protection', caster=player)+[step('resolve')]
                    w.source['bp'] = 1
                    a += w.spell('Wrath of God', 'sweep', caster=player)
                else:
                    n = 'Make a Stand' if shroud else 'Giant Growth'
                    a += w.spell(n, 'protection', () if shroud else ('source',), caster=player)+[step('resolve')]
                    w.source.update(bp=1 if shroud else 3, bt=0 if shroud else 3)
                    a += w.spell('Pyroclasm', 'sweep', caster=player)
                    w.source['damage'] = 2
                a += w.intent(['victim', 'second_victim'], partial=mode == 'partial_accept')+[step('resolve')]
                w.leave('victim'); w.leave('second_victim')
                a += w.effects()
            elif mode.startswith('keyword_') or mode.startswith('reach_'):
                if mode.endswith('after'):
                    regular_death()
                a += w.age()
                reach = mode.startswith('reach_')
                first = 'First strike' in model['keywords']
                blocker = 'Giant Spider' if reach else ('Grizzly Bears' if mode.endswith('after') else 'Eager Cadet') if first else 'Grizzly Bears'
                w.add(blocker, 'blocker', 1-player)
                if reach and model['power']+w.source['plus']+w.source['bp'] < int(oracle[blocker]['toughness']):
                    a += w.spell('Giant Growth', 'reach_growth', ('source',), caster=player)+[step('resolve')]
                    w.source.update(bp=3, bt=3)
                power = model['power']+w.source['plus']+w.source['bp']
                blocked = reach or first
                w.source['tapped'] = 1
                a += [step('begin_attackers'), step('attack', ref=w.source['ref'], value=1), step('finish_attackers'),
                      step('begin_blockers'), step('block', ref='blocker', other=w.source['ref'], value=int(blocked)), step('advance_combat')]
                if blocked:
                    if power < int(oracle[blocker]['toughness']):
                        raise ValueError('calibrated keyword combat must kill its blocker')
                    if history:
                        w.marked.add(('blocker', 0))
                    a += w.intent(['blocker'])
                    w.source['damage'] = 0 if first else int(oracle[blocker]['power'])
                    w.leave('blocker')
                else:
                    w.life[1-player] -= power
                a += [step('combat_damage'), step('resolve')]+w.effects()+[step('require_card', ref=w.source['ref'], property='in_play', value=1), step('end_combat')]
            elif mode in ('prevented_death', 'expired_history', 'source_new_object', 'victim_new_object', 'undamaged_death'):
                if mode == 'prevented_death':
                    a += w.spell('Safe Passage', 'prevention', caster=who)+[step('resolve')]
                    a += w.mark_damage(prevented=True)
                else:
                    a += w.mark_damage()
                if mode == 'expired_history':
                    a += w.age(own=False)
                elif mode in ('source_new_object', 'victim_new_object'):
                    a += w.blink('source' if mode == 'source_new_object' else 'victim')
                a += w.kill('second_victim' if mode == 'undamaged_death' else 'victim')
            else:
                if mode == 'flying_gained':
                    a += w.spell('Jump', 'flying_modifier', ('victim',), caster=player)+[step('resolve')]
                    w.entities['victim']['flying'] = True
                elif mode == 'flying_lost':
                    a += w.spell('Grounded', 'flying_modifier', ('victim',), caster=player)+[step('resolve')]
                    w.entities['flying_modifier']['attached'] = 'victim'
                    w.entities['victim'].update(flying=False, grounded=True)
                regular_death(decline=mode == 'decline')
                if mode == 'repeat':
                    regular_death('second_victim', 'second_removal')
                elif mode == 'source_blinked':
                    a += w.blink()
                    regular_death('second_victim', 'second_removal')
                elif mode == 'counter_cancel':
                    a += [step('require_card', ref=w.source['ref'], property='plus1_counters', value=1)]
                    if shroud:
                        a += w.spell("Black Sun's Zenith", 'counter_cancel', caster=player, x=1, destination='library')+[step('resolve')]
                        w.shuffles[player] += 1
                    else:
                        a += w.spell('Virulent Wound', 'counter_cancel', ('source',))+[step('resolve')]
                    w.source['plus'] -= 1
                    a += w.effects()
                elif mode == 'temporary_pump':
                    a += w.spell('Make a Stand' if shroud else 'Giant Growth', 'pump', () if shroud else ('source',), caster=player)+[step('resolve')]
                    w.source.update(bp=1 if shroud else 3, bt=0 if shroud else 3)
                    a += w.stats()+[step('cleanup'), step('refresh')]
                    w.source.update(bp=0, bt=0, damage=0)
                    a += w.stats()
                elif mode == 'cleanup_once':
                    a += [step('cleanup'), step('refresh')]
                    w.source.update(bp=0, bt=0, damage=0)
                    a += w.stats()
        checks = w.receipts()
        sequence = w.pre+a+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': death_cut}[mode]
            if cut is None:
                raise ValueError('missing real extended death-counter snapshot boundary')
            tail = a[cut:]+checks
            sequence = w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        results.append(dict(name=f'DeathCounterExtendedRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                            setup=w.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'],
                            untested_components=['AI partial-decline policy', 'independent pending trigger order/source-removal window',
                                                 'multiplayer', 'targeted blink positive is illegal on Shroud'])))
    return results
