"""Whole Modular composed with artifact entry, upkeep, grave-top costs and combat."""
import copy
import json
import re
from pathlib import Path
from death_modular_transfer_recipes import World as ModularWorld, oracle_model as modular_model
from death_counter_trigger_recipes import exact

FAMILY = 'death_modular_composition'
GAPS = ['independent death-trigger announcement/target-response window; held core policy probes are not coverage',
        'response to a pending activation; cast_response accepts spells only',
        'real token-copy producer and simultaneous counter-annihilation/death LKI remain separate pilots',
        'nonartifact creature retaining Modular: no implemented verified witness currently selected',
        'AI discretionary decline and independent trigger ordering']


def pins():
    return json.loads((Path(__file__).parent/'data/death_modular_composition_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if not isinstance(record, dict): raise ValueError('whole Modular composition required')
    lines = record.get('oracle_text', '').split('\n'); passive = []
    if lines[0] == 'Trample': passive = [lines.pop(0)]
    if len(lines) != 2: raise ValueError('one complete extra clause and Modular required')
    base = dict(record, oracle_text=lines[-1], keywords=['Modular'])
    common = modular_model(base)
    if common['sunburst'] or set(record.get('keywords', [])) != set(['Modular']+passive):
        raise ValueError('complete reviewed composition keywords required')
    text = lines[0]; generic = counter_cost = 0
    if text == 'Whenever another artifact enters, put a +1/+1 counter on this creature.' and passive == ['Trample']:
        feature = 'artifact_entry'
    elif text == 'At the beginning of your upkeep, put a +1/+1 counter on each creature you control with modular.' and not passive:
        feature = 'upkeep_team'
    elif text == 'Whenever this creature deals combat damage to a player, put a +1/+1 counter on it.' and not passive:
        feature = 'combat_player'
    else:
        match = re.fullmatch(r'(?:\{([1-9])\}, )?Remove (a|two) \+1/\+1 counters? from this creature: Put target artifact card from your graveyard on top of your library\.', text)
        if not match or passive: raise ValueError('complete reviewed extra Modular clause required')
        feature = 'grave_top'; generic = int(match[1] or 0); counter_cost = 1 if match[2] == 'a' else 2
    return dict(initial_counters=common['initial_counters'], keywords=passive, feature=feature,
                activation_generic=generic, counter_cost=counter_cost)


def spec(d, record):
    from recipes import cost_from_oracle
    expected = dict(schema=1, **oracle_model(record))
    if d.get('family') != FAMILY or d.get('version') != 2 or d.get('oracle_text') != record.get('oracle_text') or not exact(d.get('params'), expected):
        raise ValueError('typed complete Modular composition declaration required')
    model = dict(copy.deepcopy(expected), family=FAMILY, mana=cost_from_oracle(record))
    if not model['mana'][0] or any(model['mana'][1:]): raise ValueError('reviewed positive generic source cost required')
    selected = d.get('variants')
    if not isinstance(selected, list) or not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit unique supported composition variants required')
    return model


def variants(m):
    modes = ['cast_only','payment','generic_colored','insufficient_mana','countered','transfer','recipient_enemy',
             'nonartifact_recipient','noncreature_recipient','recipient_shrouded','source_foreign_owner',
             'source_bounced','recast_bounced','source_exiled','other_dies','source_added_counter','recipient_added_counter',
             'minus_counter','zero_toughness','lethal_damage','combat_death','sacrifice','cleanup_once','bare_zones',
             'ready_snapshot','pending_snapshot','death_snapshot','resolved_snapshot']
    modes += {'artifact_entry':['artifact_own_entry','artifact_enemy_entry','nonartifact_entry','repeat_entry',
                               'entry_after_source_removed','entry_then_transfer','trample_overflow','trample_prevented'],
              'upkeep_team':['own_upkeep_team','opponent_upkeep','repeat_upkeep','source_enters_after_upkeep',
                             'source_removed_before_upkeep','upkeep_then_transfer','upkeep_snapshot'],
              'grave_top':['activate_fresh','activate_tapped','artifact_creature_top','enemy_grave_only','empty_own_grave',
                           'activation_then_transfer','last_counter_death','counterless_cannot_activate','controller_grave',
                           'activation_no_mana_fee','activation_pending_snapshot','activation_resolved_snapshot'],
              'combat_player':['combat_player_hit','blocked_creature_hit','prevented_player_hit','noncombat_source_hit',
                               'combat_hit_then_transfer','repeat_combat_hit','combat_snapshot']}[m['feature']]
    return [f'{mode}_{p}' for p in (0,1) for mode in modes]+['decline_0']


def validate_witnesses(oracle, registry):
    data = pins()
    for n, record in data['cards'].items():
        if not exact(record, oracle.get(n)) or not exact(data['registry'][n], registry.get(n)):
            raise ValueError('complete Modular composition witness changed: '+n)
    for n, s in data['sources'].items():
        row = registry.get(n, {})
        if not exact(s['oracle'], oracle.get(n)) or row.get('csvid') != s['csvid'] or row.get('handler') != s['handler'] or row.get('kind') != 'card' or row.get('handler') in ('NULL','0','card_dummy'):
            raise ValueError('implemented normal composition source changed: '+n)
        oracle_model(oracle[n])


class World(ModularWorld):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.forced_targets = 0; self.current = self.controller

    def phase(self, player):
        from recipes import step
        self.current = player
        return [step('turn', player=player, value=0)]

    def entered(self, key):
        if key != 'source' and self.source['alive'] and self.model['feature'] == 'artifact_entry' and 'Artifact' in self.oracle[self.entities[key]['name']]['type_line']:
            self.source['plus'] += 1

    def permanent(self, name, key, player, counters=0):
        from recipes import step, cost_from_oracle
        if player != self.current: run = self.phase(player)
        else: run = []
        if 'Creature' in self.oracle[name]['type_line']:
            self.add(name, key, player, 'hand')
            run += self.cast_creature(key, cost_from_oracle(self.oracle[name]), counters=counters)
        else:
            run += self.spell(name, key, caster=player)+[step('resolve'), step('assert_choices_consumed')]
        self.entered(key)
        return run+self.effects()

    def visit(self, player):
        from recipes import step
        if any(any(pool) for pool in self.mana): raise ValueError('real upkeep requires actually spent pools')
        run = [step('advance_turn') for _ in range(2 if self.current == player else 1)]
        self.current = player
        for e in self.entities.values():
            e.update(damage=0, bp=0, bt=0)
            if e['alive'] and e['player'] == player: e['tapped'] = 0
        run += [step('untap_step', player=player), step('upkeep_step', player=player)]
        if self.source['alive'] and self.model['feature'] == 'upkeep_team' and self.source['player'] == player:
            for e in self.entities.values():
                if e['alive'] and e['player'] == player and 'Creature' in self.oracle[e['name']]['type_line'] and 'Modular' in self.oracle[e['name']].get('keywords', []):
                    e['plus'] += 1
        return run+self.effects()+self.phase(player)

    def age(self, own=True):
        # Overseer's next own turn includes its printed upkeep, rather than
        # silently skipping that ability while making the creature combat-ready.
        if self.model['feature'] == 'upkeep_team':
            return self.visit(self.controller if own else 1-self.controller)
        return super().age(own=own)

    def kill(self, spell='Murder', key='death_spell', decline=False, x=None):
        from recipes import step
        run = self.spell(spell, key, ('source',), x=x)
        pending = len(run)
        run += self.intent(decline); self.die(decline)
        return run+[step('resolve'), step('assert_choices_consumed')]+self.effects(), pending

    def reclaim(self, target='Sol Ring'):
        from recipes import step
        p = self.controller; cost = self.model['counter_cost']; mana = self.model['activation_generic']
        if self.source['plus'] < cost or target not in self.zones['graveyard'][p]: raise ValueError('real available counter and own grave target required')
        run = [step('require_can_activate', ref=self.source['ref'], value=1),
               step('choose_zone_card', player=p, value=self.registry[target]['csvid'])]
        if mana:
            run = self.fund(p, [mana,0,0,0,0,0])+run; self.mana[p][0] -= mana
        self.source['plus'] -= cost
        dying = self.source['plus']+self.source['bt'] <= 0
        if dying: run += self.intent(); self.die()
        run += [step('activate', ref=self.source['ref'], value=1), step('assert_stack', value=1)]
        pending = len(run)
        self.zones['graveyard'][p].remove(target); self.zones['library'][p].insert(0,target)
        return run+[step('resolve'), step('assert_choices_consumed')]+self.effects(), pending

    def draw_top(self, key='draw'):
        from recipes import step
        p = self.controller
        run = self.spell('Reach Through Mists', key, caster=p)+[step('resolve'), step('assert_choices_consumed')]
        self.zones['hand'][p].append(self.zones['library'][p].pop(0))
        return run+self.effects()

    def combat(self, blocker=None, prevent=False, slith=False, fresh=False):
        from recipes import step
        p = self.controller; run = []
        if not fresh:
            if self.current != p: run += self.phase(p)
            run += self.age(); self.current = p
        if prevent:
            run += self.spell('Safe Passage', 'prevention', caster=1-p)+[step('resolve'), step('assert_choices_consumed')]
        power = self.source['plus']+self.source['bp']; toughness = self.source['plus']+self.source['bt']
        run += [step('begin_attackers'), step('attack', ref=self.source['ref'], value=1), step('finish_attackers'), step('begin_blockers')]
        self.source['tapped'] = 1
        if blocker:
            run += [step('block', ref=self.entities[blocker]['ref'], other=self.source['ref'], value=1)]
            b = self.entities[blocker]; br = self.oracle[b['name']]
            b['damage'] = min(power, int(br['toughness'])) if 'Trample' in self.model['keywords'] else power
            self.source['damage'] = int(br['power'])
            player_damage = max(0,power-int(br['toughness'])) if 'Trample' in self.model['keywords'] else 0
            if prevent: b['damage'] = 0; player_damage = 0
            if b['damage'] >= int(br['toughness']): self.leave(blocker)
            if self.source['damage'] >= toughness: run += self.intent(); self.die()
        else: player_damage = 0 if prevent else power
        self.life[1-p] -= player_damage
        if slith and player_damage and self.source['alive']: self.source['plus'] += 1
        run += [step('advance_combat'), step('combat_damage'), step('resolve'), step('assert_choices_consumed')]+self.effects()+[step('end_combat')]
        return run


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, cost_from_oracle
    validate_witnesses(oracle, registry)
    if name not in pins()['sources'] or pins()['sources'][name]['csvid'] != csvid: raise ValueError('unreserved composition primary')
    if not selected or len(selected) != len(set(selected)) or set(selected)-set(variants(model)): raise ValueError('unsupported composition selection')
    results = []
    for variant in selected:
        mode, p = variant.rsplit('_',1); p = int(p); feature = model['feature']
        w = World(name,p,model,registry,oracle,bare=mode=='bare_zones')
        foreign = mode in ('source_foreign_owner','controller_grave'); owner = 1-p if foreign else p
        w.add(name,'source',owner,'hand')
        recipient = {'nonartifact_recipient':'Grizzly Bears','noncreature_recipient':'Sol Ring'}.get(mode,'Memnite')
        w.add(recipient,'recipient',1-p if mode in ('recipient_enemy','sacrifice') else p)
        if mode == 'other_dies': w.add('Grizzly Bears','other',p)
        if mode == 'sacrifice': w.add("Ashnod's Altar",'altar',p)
        if mode in ('combat_death','trample_overflow','trample_prevented','blocked_creature_hit'):
            w.add(('Fusion Elemental' if model['initial_counters']>=6 else 'Craw Wurm') if mode=='combat_death' else 'Memnite' if feature=='artifact_entry' else 'Ornithopter','blocker',1-p)
        if feature == 'grave_top' and mode in ('artifact_creature_top','last_counter_death','counterless_cannot_activate'):
            w.add('Memnite','grave_artifact',p,'graveyard')
        if mode in ('enemy_grave_only','empty_own_grave'):
            keep = lambda n:mode=='enemy_grave_only' and 'Artifact' not in oracle[n]['type_line']
            w.zones['graveyard'][p] = [n for n in w.zones['graveyard'][p] if keep(n)]
            w.setup = [c for c in w.setup if c['zone']!='graveyard' or c['player']!=p or keep(c['name'])]
        a = []; ready = pending = death = feature_cut = None; cost = model['mana']; entry = model['initial_counters']
        if mode == 'source_enters_after_upkeep': a += w.visit(p)
        if owner != p: a += w.phase(owner)
        pool = [0,0,0,sum(cost),0,0] if mode=='generic_colored' else list(cost)
        if mode == 'insufficient_mana':
            a += w.fund(p,[sum(cost)-1,0,0,0,0,0])+[step('cast',ref='source',value=0),step('assert_stack',value=0)]
        elif mode == 'countered':
            a += w.cast_creature('source',pool,resolve=False)+[step('announce_spell',ref='source')]
            q=1-p; w.add('Counterspell','counter',q,'hand')
            a += w.fund(q,cost_from_oracle(oracle['Counterspell']))+[step('cast_response',ref='counter',value=1),step('resolve_response'),step('assert_choices_consumed')]
            w.zones['hand'][q].remove('Counterspell'); w.zones['graveyard'][q].append('Counterspell'); w.zones['graveyard'][p].append(name); w.mana[q]=[0]*6
        else:
            a += w.cast_creature('source',pool,counters=entry)+w.effects()
            if foreign:
                a += w.phase(p)+w.spell('Control Magic','control',('source',),caster=p)+[step('resolve'),step('assert_choices_consumed')]
                w.source.update(player=p,ref='controlled_source'); w.entities['control']['attached']='source'
                a += [step('bind_permanent',ref=w.source['ref'],player=p,value=csvid)]+w.effects()
            ready = len(a)
            if mode in ('cast_only','payment','generic_colored','source_enters_after_upkeep'): pass
            elif mode in ('source_bounced','source_exiled'):
                a += w.spell('Boomerang' if mode=='source_bounced' else 'Swords to Plowshares','departure',('source',))+[step('resolve'),step('assert_choices_consumed')]
                if mode=='source_exiled': w.life[p] += w.source['plus']
                w.leave('source','hand' if mode=='source_bounced' else 'exile'); a += w.effects()
            elif mode == 'other_dies':
                a += w.spell('Murder','other_removal',('other',))+[step('resolve'),step('assert_choices_consumed')]; w.leave('other'); a += w.effects()
            elif mode in ('artifact_own_entry','artifact_enemy_entry','nonartifact_entry','repeat_entry','entry_after_source_removed','entry_then_transfer','trample_overflow','trample_prevented'):
                if mode=='entry_after_source_removed': run,_=w.kill();a+=run
                a += w.permanent('Grizzly Bears' if mode=='nonartifact_entry' else 'Sol Ring','entry_witness',1-p if mode=='artifact_enemy_entry' else p)
                if mode=='repeat_entry': a += w.permanent('Arcbound Worker','entry_second',p,counters=1)
                if mode in ('trample_overflow','trample_prevented'): a += w.combat('blocker',prevent=mode=='trample_prevented')
                if mode in ('entry_then_transfer','trample_overflow','trample_prevented'): run,_=w.kill(key='earned_death');a+=run
            elif mode in ('own_upkeep_team','opponent_upkeep','repeat_upkeep','source_removed_before_upkeep','upkeep_then_transfer','upkeep_snapshot'):
                if mode in ('own_upkeep_team','upkeep_snapshot'):
                    a += w.permanent('Arcbound Worker','own_modular',p,counters=1)
                    a += w.permanent('Arcbound Worker','enemy_modular',1-p,counters=1)+w.phase(p)
                    w.add('Grizzly Bears','nonmodular',p)
                if mode=='source_removed_before_upkeep': run,_=w.kill();a+=run
                feature_cut = len(a)
                a += w.visit(1-p if mode=='opponent_upkeep' else p)
                if mode=='repeat_upkeep': a += w.visit(p)
                if mode=='upkeep_then_transfer': run,_=w.kill();a+=run
            elif mode in ('activate_fresh','activate_tapped','artifact_creature_top','enemy_grave_only','empty_own_grave','activation_then_transfer','last_counter_death','counterless_cannot_activate','controller_grave','activation_no_mana_fee','activation_pending_snapshot','activation_resolved_snapshot'):
                if mode in ('enemy_grave_only','empty_own_grave'):
                    a += [step('require_can_activate',ref=w.source['ref'],value=0)]
                else:
                    if mode=='activate_tapped': a += w.combat()
                    if mode=='counterless_cannot_activate':
                        a += w.spell('Giant Growth','sustain',('source',),caster=p)+[step('resolve'),step('assert_choices_consumed')]
                        w.source.update(bp=3,bt=3);a+=w.effects()
                    if mode=='activation_no_mana_fee': a += w.fund(p,[2,0,0,0,0,0])
                    run,offset = w.reclaim('Memnite' if mode=='artifact_creature_top' else 'Sol Ring'); feature_cut=len(a)+offset;a+=run
                    if mode in ('last_counter_death','counterless_cannot_activate'):
                        run,_=w.reclaim('Memnite');a+=run
                    if mode=='counterless_cannot_activate': a += [step('require_can_activate',ref=w.source['ref'],value=0)]
                    if mode!='activation_no_mana_fee': a += w.draw_top()
                    if mode in ('activation_then_transfer','controller_grave','activate_fresh','activate_tapped'): run,_=w.kill(key='after_activation_death');a+=run
            elif mode in ('combat_player_hit','blocked_creature_hit','prevented_player_hit','noncombat_source_hit','combat_hit_then_transfer','repeat_combat_hit','combat_snapshot'):
                feature_cut=len(a)
                if mode=='noncombat_source_hit':
                    a += w.spell("Soul's Fire",'noncombat',('source',),caster=p)
                    # The second actual target is a player, not a permanent fallback.
                    at=next(i for i in range(len(a)-1,-1,-1) if a[i]['op']=='cast' and a[i].get('ref')=='noncombat')
                    a.insert(at,step('choose_player',player=p,value=1-p))
                    a += [step('resolve'),step('assert_choices_consumed')];w.life[1-p]-=w.source['plus'];a+=w.effects()
                else:
                    a += w.combat('blocker' if mode=='blocked_creature_hit' else None,prevent=mode=='prevented_player_hit',slith=True)
                    if mode=='repeat_combat_hit': a += w.combat(slith=True)
                if mode in ('combat_hit_then_transfer','combat_player_hit','noncombat_source_hit'): run,_=w.kill(key='after_damage_death');a+=run
            else:
                if mode=='recipient_shrouded':
                    a += w.spell('Robe of Mirrors','shroud_aura',('recipient',),caster=p)+[step('resolve'),step('assert_choices_consumed')]
                    w.entities['shroud_aura']['attached']='recipient';w.entities['recipient']['shroud']=True
                if mode in ('source_added_counter','recipient_added_counter','minus_counter','recast_bounced'):
                    key='recipient' if mode=='recipient_added_counter' else 'source'
                    a += w.spell('Burst of Strength','growth',(key,),caster=p)+[step('resolve'),step('assert_choices_consumed')];w.entities[key]['plus']+=1;a+=w.effects()
                if mode=='minus_counter':
                    w.add('Fume Spitter','spitter',p,'hand');a+=w.cast_creature('spitter',cost_from_oracle(oracle['Fume Spitter']))
                    a += [step('choose_target',player=p,ref=w.source['ref']),step('activate',ref='spitter',value=1),step('resolve'),step('assert_choices_consumed')]
                    w.leave('spitter');w.source['plus']-=1;a+=w.effects()
                if mode=='recast_bounced':
                    a+=w.spell('Boomerang','bounce',('source',))+[step('resolve'),step('assert_choices_consumed')];w.leave('source','hand');a+=w.effects()
                    w.source.update(ref='recast_source',plus=0,minus=0,bp=0,bt=0,damage=0,tapped=0)
                    a += [step('bind_hand',ref=w.source['ref'],player=p,value=csvid)]+w.cast_creature('source',cost,counters=entry)+w.effects()
                if mode=='sacrifice':
                    if p==0:a+=[step('choose_target',player=p,ref=w.source['ref'])]
                    a+=w.intent();w.die();w.mana[p][0]+=2
                    a += [step('activate',ref='altar',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]+w.effects()
                elif mode=='combat_death':a+=w.combat('blocker')
                else:
                    spell='Death Wind' if mode=='zero_toughness' else 'Heat Ray' if mode=='lethal_damage' else 'Murder'
                    run,offset=w.kill(spell,x=w.source['plus'] if mode in ('zero_toughness','lethal_damage') else None,decline=mode=='decline');pending=len(a)+offset;a+=run;death=len(a)
                    if mode in ('cleanup_once','death_snapshot'):a += [step('cleanup'),step('refresh')]+w.effects()
        checks=w.receipts()+w.effects();sequence=w.pre+a+checks
        cuts={'ready_snapshot':ready,'pending_snapshot':pending,'death_snapshot':death,'upkeep_snapshot':feature_cut,'combat_snapshot':feature_cut,'activation_pending_snapshot':feature_cut}
        if mode in cuts:
            cut=cuts[mode]
            if cut is None:raise ValueError('real composition snapshot boundary absent')
            tail=a[cut:]+checks
            sequence=w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode in ('resolved_snapshot','activation_resolved_snapshot'):
            sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        results.append(dict(name=f'DeathModularCompositionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=sequence,
                            _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=GAPS)))
    return results
