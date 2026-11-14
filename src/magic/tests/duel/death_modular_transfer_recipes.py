"""Whole printed Modular through real entry, payment, death and counter transfer."""
import copy
import json
import re
from pathlib import Path
from death_counter_extended_recipes import World as ZoneWorld
from death_counter_trigger_recipes import exact

FAMILY = 'death_modular_transfer'
GAPS = ['independent postdeath trigger announcement/response boundary',
        'AI discretionary decline and independent simultaneous trigger ordering',
        'real token-copy producer and copied-object entry require separate calibration',
        'simultaneous death/counter-annihilation LKI edge held until core protocol calibration']


def pins():
    return json.loads((Path(__file__).parent/'data/death_modular_transfer_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record, dict) or record.get('layout') != 'normal' or record.get('card_faces') or
            record.get('selected_face') or not record.get('type_line', '').startswith('Artifact Creature — ') or
            record.get('power') != '0' or record.get('toughness') != '0'):
        raise ValueError('complete normal printed0/0 artifact creature required')
    lines = record.get('oracle_text', '').split('\n')
    keywords = list(record.get('keywords', []))
    passive = []
    if lines[0] in ('Flying', 'Haste'):
        passive = [lines.pop(0)]
    if len(lines) != 1:
        raise ValueError('whole reviewed Modular clause and keyword required')
    tail = 'When it dies, you may put its +1/+1 counters on target artifact creature.)'
    m = re.fullmatch(r'Modular ([1-9]) \(This creature enters with (a|two|three|four|five|six|seven|eight|nine) \+1/\+1 counters? on it\. '+re.escape(tail), lines[0])
    sunburst = lines[0] == ('Modular—Sunburst (This creature enters with a +1/+1 counter on it for each color of mana spent to cast it. '+tail)
    if m:
        words = ['a', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine']
        initial = int(m[1])
        if words[initial-1] != m[2]:
            raise ValueError('printed numeral and reminder counter count disagree')
    elif sunburst and not passive:
        initial = 0
    else:
        raise ValueError('complete reviewed Modular/Sunburst clause required')
    if set(keywords) != set(passive+['Modular']+(['Sunburst'] if sunburst else [])) or len(keywords) != len(set(keywords)):
        raise ValueError('whole printed keyword identity required')
    return dict(initial_counters=initial, sunburst=sunburst, keywords=passive)


def spec(d, record):
    from recipes import cost_from_oracle
    expected = dict(schema=1, **oracle_model(record))
    if (d.get('family') != FAMILY or d.get('version') != 2 or d.get('oracle_text') != record.get('oracle_text') or
            not exact(d.get('params'), expected)):
        raise ValueError('typed complete Modular declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record))
    if any(model['mana'][1:]) or not model['mana'][0]:
        raise ValueError('reviewed positive generic printed casting cost required')
    if model['sunburst'] and model['mana'][0] != 6:
        raise ValueError('reviewed full Sunburst payment required')
    selected = d.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported Modular variants required')
    return model


def variants(m):
    modes = ['cast_only', 'payment', 'insufficient_mana', 'countered', 'transfer', 'recipient_enemy',
             'source_foreign_owner', 'nonartifact_recipient', 'noncreature_recipient', 'recipient_shrouded',
             'source_bounced', 'recast_bounced', 'source_exiled', 'other_dies', 'source_added_counter', 'recipient_added_counter',
             'minus_counter', 'zero_toughness', 'lethal_damage', 'combat_death', 'sacrifice', 'cleanup_once',
             'bare_zones', 'ready_snapshot', 'pending_snapshot', 'death_snapshot', 'resolved_snapshot']
    modes += ['sunburst_zero', 'sunburst_two', 'sunburst_five'] if m['sunburst'] else ['generic_colored']
    if 'Haste' in m['keywords']:
        modes += ['fresh_attack']
    if 'Flying' in m['keywords']:
        modes += ['flying_unblocked']
    return [f'{mode}_{p}' for p in (0, 1) for mode in modes]+['decline_0']


def validate_witnesses(oracle, registry):
    data = pins()
    for n, r in data['cards'].items():
        if not exact(r, oracle.get(n)) or not exact(data['registry'][n], registry.get(n)):
            raise ValueError('complete Modular witness changed: '+n)
    for n, s in data['sources'].items():
        row = registry.get(n, {})
        if (not exact(s['oracle'], oracle.get(n)) or row.get('csvid') != s['csvid'] or row.get('handler') != s['handler'] or
                row.get('kind') != 'card' or row.get('handler') in ('NULL', '0', 'card_dummy')):
            raise ValueError('implemented normal Modular primary changed: '+n)
        oracle_model(oracle[n])


class World(ZoneWorld):
    def effects(self):
        from recipes import step
        return [s for k, e in self.entities.items() if e['alive'] for s in self.stats(k)]+[
            step('assert_life', player=p, value=self.life[p]) for p in (0, 1)]+[
            step('assert_unused_choices', value=0), step('assert_forced_target_choices', value=self.forced_targets)]+self.mana_checks()

    def cast_creature(self, key, pool, counters=0, resolve=True):
        from recipes import step, observe, cost_from_oracle
        e = self.entities[key]; p = e['player']
        run = self.fund(p, pool)
        cost = cost_from_oracle(self.oracle[e['name']])
        if p == 0 and cost[0]:
            # Human colored mana paid toward generic requires actual pool clicks.
            generic = cost[0]-min(cost[0], pool[0])
            for color in range(1, 6):
                number = min(generic, max(0, pool[color]-cost[color]))
                run += [step('choose_mana', player=p, color=color) for _ in range(number)]
                generic -= number
        run += [step('cast', ref=e['ref'], value=1), step('assert_stack', value=1), observe(e['ref'], 'in_hand', 0)]
        self.zones['hand'][p].remove(e['name'])
        self.mana[p] = [0]*6  # Every reviewed successful pool equals the printed total cost.
        if resolve:
            run += [step('resolve'), step('assert_choices_consumed')]
            e.update(alive=True, plus=counters)
        return run

    def eligible(self):
        r = self.entities['recipient']; types = self.oracle[r['name']]['type_line'].partition(' — ')[0].split()
        return r['alive'] and 'Artifact' in types and 'Creature' in types and not r.get('shroud')

    def intent(self, decline=False, policy=False):
        from recipes import step
        # The actual Modular death legacy is mandatory2 and calls its target
        # selector directly. There is no optional source-priority offer here.
        run = []
        if self.eligible():
            if policy: self.forced_targets += 1
            run += [step('choose_optional_target' if decline or policy else 'choose_target', ref=self.entities['recipient']['ref'], player=self.controller)]
        return run

    def die(self, decline=False):
        amount = self.source['plus']
        self.leave('source')
        if self.eligible() and not decline:
            self.entities['recipient']['plus'] += amount


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, cost_from_oracle
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid:
        raise ValueError('unreserved Modular primary')
    if not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('unsupported Modular selection')
    results = []
    for variant in selected:
        mode, p = variant.rsplit('_', 1); p = int(p)
        w = World(name, p, model, registry, oracle, bare=mode == 'bare_zones')
        w.forced_targets = 0
        owner = 1-p if mode == 'source_foreign_owner' else p
        w.add(name, 'source', owner, 'hand')
        recipient = {'nonartifact_recipient': 'Grizzly Bears', 'noncreature_recipient': 'Sol Ring'}.get(mode, 'Memnite')
        w.add(recipient, 'recipient', 1-p if mode in ('recipient_enemy', 'sacrifice') else p)
        if mode == 'other_dies':
            w.add('Grizzly Bears', 'unrelated', p)
        if mode == 'sacrifice':
            w.add("Ashnod's Altar", 'altar', p)
        if mode == 'combat_death':
            w.add('Giant Spider' if 'Flying' in model['keywords'] else 'Craw Wurm', 'blocker', 1-p)
        if mode == 'flying_unblocked':
            w.add('Grizzly Bears', 'blocker', 1-p)
        cost = model['mana']; total = sum(cost)
        pool = list(cost); entry = model['initial_counters']
        if model['sunburst']:
            pool = [0, 0, 0, total, 0, 0]; entry = 1
            if mode == 'sunburst_zero': pool = [total, 0, 0, 0, 0, 0]; entry = 0
            elif mode == 'sunburst_two': pool = [0, 3, 0, 3, 0, 0]; entry = 2
            elif mode == 'sunburst_five': pool = [1, 1, 1, 1, 1, 1]; entry = 5
        elif mode == 'generic_colored':
            pool = [0, 0, 0, total, 0, 0]
        a = []; ready_cut = pending_cut = death_cut = None
        if owner != p:
            a += [step('turn', player=owner, value=0)]
        if mode == 'insufficient_mana':
            poor = [max(0, total-1), 0, 0, 0, 0, 0]
            a += w.fund(p, poor)+[step('cast', ref='source', value=0), step('assert_stack', value=0)]
        elif mode == 'countered':
            a += w.cast_creature('source', pool, resolve=False)+[step('announce_spell', ref='source')]
            q = 1-p; w.add('Counterspell', 'counter', q, 'hand'); cc = cost_from_oracle(oracle['Counterspell'])
            a += w.fund(q, cc)+[step('cast_response', ref='counter', value=1), step('resolve_response'), step('assert_choices_consumed')]
            w.zones['hand'][q].remove('Counterspell'); w.zones['graveyard'][q].append('Counterspell'); w.zones['graveyard'][p].append(name); w.mana[q] = [0]*6
        else:
            if mode == 'sunburst_zero':
                a += w.cast_creature('source', pool, resolve=False)+w.intent()+[step('resolve'), step('assert_choices_consumed')]
            else:
                a += w.cast_creature('source', pool, counters=entry)
            if entry == 0:
                # A genuinely colorless-paid0/0 dies on entry. No live binding or fake counters.
                w.die()
            a += w.effects()
            if mode == 'source_foreign_owner':
                a += [step('turn', player=p, value=0)]
                a += w.spell('Control Magic', 'control', ('source',), caster=p)+[step('resolve'), step('assert_choices_consumed')]
                w.source.update(player=p, ref='controlled_source')
                w.entities['control']['attached'] = 'source'
                a += [step('bind_permanent', ref='controlled_source', player=p, value=csvid)]+w.effects()
            ready_cut = len(a)

            def kill(removal='Murder', decline=False, policy=False):
                nonlocal pending_cut, death_cut
                run = w.spell(removal, 'death_spell', ('source',))
                pending_cut = len(a)+len(run)
                run += w.intent(decline, policy)
                if removal == 'Disfigure':
                    w.source.update(bp=-2, bt=-2)
                elif removal == 'Lightning Bolt':
                    w.source['damage'] = 3
                w.die(decline)
                return run+[step('resolve'), step('assert_choices_consumed')]+w.effects()

            if mode in ('cast_only', 'payment', 'generic_colored', 'sunburst_zero'):
                pass
            elif mode in ('source_bounced', 'source_exiled'):
                spell = 'Boomerang' if mode == 'source_bounced' else 'Swords to Plowshares'
                a += w.spell(spell, 'departure', ('source',))+[step('resolve'), step('assert_choices_consumed')]
                if mode == 'source_exiled': w.life[p] += w.source['plus']
                w.leave('source', 'hand' if mode == 'source_bounced' else 'exile')
                a += w.effects()
            elif mode == 'other_dies':
                a += w.spell('Murder', 'other_removal', ('unrelated',))+[step('resolve'), step('assert_choices_consumed')]
                w.leave('unrelated'); a += w.effects()
            else:
                if mode == 'recipient_shrouded':
                    a += w.spell('Robe of Mirrors', 'shroud_aura', ('recipient',), caster=p)+[step('resolve'), step('assert_choices_consumed')]
                    w.entities['shroud_aura']['attached'] = 'recipient'; w.entities['recipient']['shroud'] = True
                if mode in ('source_added_counter', 'recipient_added_counter', 'minus_counter', 'recast_bounced'):
                    key = 'recipient' if mode == 'recipient_added_counter' else 'source'
                    a += w.spell('Burst of Strength', 'growth', (key,), caster=p)+[step('resolve'), step('assert_choices_consumed')]
                    w.entities[key]['plus'] += 1; a += w.effects()
                if mode == 'recast_bounced':
                    a += w.spell('Boomerang', 'bounce', ('source',))+[step('resolve'), step('assert_choices_consumed')]
                    w.leave('source', 'hand'); a += w.effects()
                    w.source.update(ref='recast_source', plus=0, minus=0, bp=0, bt=0, damage=0, tapped=0)
                    a += [step('bind_hand', ref='recast_source', player=p, value=csvid)]
                    a += w.cast_creature('source', pool, counters=entry)+w.effects()
                if mode == 'minus_counter':
                    w.add('Fume Spitter', 'spitter', p, 'hand')
                    a += w.cast_creature('spitter', cost_from_oracle(oracle['Fume Spitter']))
                    a += [step('choose_target', player=p, ref=w.source['ref']), step('activate', ref='spitter', value=1), step('resolve'), step('assert_choices_consumed')]
                    w.leave('spitter'); w.source['plus'] -= 1
                    a += w.effects()
                if mode == 'sacrifice':
                    if p == 0: a += [step('choose_target', player=p, ref=w.source['ref'])]
                    a += w.intent(); w.die(); w.mana[p][0] += 2
                    a += [step('activate', ref='altar', value=1), step('resolve'), step('refresh'), step('assert_choices_consumed')]+w.effects()
                elif mode == 'combat_death':
                    a += w.age()+[step('begin_attackers'), step('attack', ref=w.source['ref'], value=1), step('finish_attackers'), step('begin_blockers'), step('block', ref='blocker', other=w.source['ref'], value=1), step('advance_combat')]
                    w.entities['blocker']['damage'] = w.source['plus']; a += w.intent(); w.die()
                    a += [step('combat_damage'), step('resolve'), step('assert_choices_consumed')]+w.effects()+[step('end_combat')]
                else:
                    if mode in ('fresh_attack', 'flying_unblocked'):
                        if mode == 'flying_unblocked': a += w.age()
                        a += [step('begin_attackers'), step('attack', ref=w.source['ref'], value=1), step('finish_attackers'), step('begin_blockers')]
                        if mode == 'flying_unblocked': a += [step('block', ref='blocker', other=w.source['ref'], value=0)]
                        a += [step('advance_combat'), step('combat_damage'), step('resolve'), step('assert_choices_consumed')]
                        w.source['tapped'] = 1; w.life[1-p] -= w.source['plus']; a += w.effects()+[step('end_combat')]
                    a += kill('Disfigure' if mode == 'zero_toughness' else 'Lightning Bolt' if mode == 'lethal_damage' else 'Murder', decline=mode == 'decline', policy=mode == 'target_policy')
                    death_cut = len(a)
                    if mode in ('cleanup_once', 'death_snapshot'):
                        a += [step('cleanup'), step('refresh')]+w.effects()
        checks = w.receipts()+w.effects()
        sequence = w.pre+a+checks
        if mode in ('ready_snapshot', 'pending_snapshot', 'death_snapshot'):
            cut = {'ready_snapshot': ready_cut, 'pending_snapshot': pending_cut, 'death_snapshot': death_cut}[mode]
            if cut is None: raise ValueError('real Modular snapshot boundary absent')
            tail = a[cut:]+checks
            sequence = w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'), step('ai_roundtrip'), step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'), step('record_state'), step('ai_roundtrip'), step('restore'), step('assert_recorded_state')]
        results.append(dict(name=f'DeathModularTransferRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                            setup=w.setup, steps=sequence, _recipe=dict(family=FAMILY, version=2, variant=variant,
                            params=declaration['params'], oracle_text=declaration['oracle_text'], scope_gaps=GAPS)))
    return results
