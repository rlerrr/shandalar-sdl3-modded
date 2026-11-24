"""One creature sacrifice per qualifying death, with whole Oracle keyword coverage."""
import copy
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from death_modular_transfer_recipes import World as ModularWorld


FAMILY = 'death_triggered_edict'
CLAUSES = {
    'When this creature dies, each player sacrifices a creature of their choice.': ('self', 'both', None),
    'Whenever this creature or another creature you control dies, each opponent sacrifices a creature of their choice.': ('controlled_with_self', 'other', 'Flying'),
    'Whenever a creature you control dies, each opponent sacrifices a creature of their choice.': ('controlled', 'other', 'Flash'),
    'Whenever a creature you control dies, each other player sacrifices a creature of their choice.': ('controlled', 'other', None),
}
GAPS = ['multiplayer choice ordering beyond two players',
        'independent response or order selection between multiple death triggers',
        'AI discretionary selection among distinct legal sacrifice candidates']


def pins():
    return json.loads((Path(__file__).parent/'data/death_triggered_edict_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('normal whole death edict required')
    lines = record.get('oracle_text', '').split('\n')
    if not lines or lines[-1] not in CLAUSES:
        raise ValueError('one reviewed whole death edict clause required')
    predicate, recipient, keyword = CLAUSES[lines[-1]]
    keywords = [keyword] if keyword else []
    creature = predicate in ('self', 'controlled_with_self')
    if (lines[:-1] != keywords or record.get('keywords') != keywords or
            (record.get('type_line', '').split(' — ')[0] != ('Creature' if creature else 'Enchantment'))):
        raise ValueError('whole reviewed source type and printed keyword required')
    return dict(predicate=predicate, recipient=recipient, keywords=keywords, creature=creature)


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = dict(schema=1, **oracle_model(record))
    if (declaration.get('family') != FAMILY or declaration.get('version') != 2 or
            declaration.get('oracle_text') != record.get('oracle_text') or not exact(expected, declaration.get('params'))):
        raise ValueError('typed complete death edict declaration required')
    model = dict(expected, family=FAMILY, mana=cost_from_oracle(record))
    chosen = declaration.get('variants')
    if not isinstance(chosen, list) or not chosen or len(chosen) != len(set(chosen)) or set(chosen)-set(variants(model)):
        raise ValueError('explicit unique supported death edict variants required')
    return model


def variants(model):
    modes = ['effect', 'no_recipient_creatures', 'hexproof_recipient', 'indestructible_recipient', 'player_shroud',
             'stolen_recipient', 'token_recipient', 'stolen_source', 'other_death', 'bounce', 'exile',
             'payment', 'unaffordable', 'countered', 'ready_snapshot', 'pending_snapshot', 'resolved_snapshot',
             'simultaneous_damage']
    if model['predicate'] == 'self':
        modes += ['one_side_only', 'opposing_turn_death', 'simultaneous_source', 'self_sacrifice']
    else:
        modes += ['stolen_death', 'lent_death', 'token_death', 'noncreature_death', 'source_departed', 'simultaneous',
                  'simultaneous_source', 'sacrifice_cost_death']
    if model['predicate'] == 'controlled_with_self': modes += ['source_dies', 'flying_unblocked', 'flying_reach']
    if 'Flash' in model['keywords']: modes += ['flash_before_death', 'flash_after_death']
    result = [f'{mode}_{p}' for p in (0,1) for mode in modes]
    result += ['chooser_alternate_0' if model['recipient'] == 'both' else 'chooser_alternate_1']
    return result


def validate_witnesses(oracle, registry):
    data = pins()
    for name, record in data['cards'].items():
        if not exact(record, oracle.get(name)) or not exact(data['registry'][name], registry.get(name)):
            raise ValueError('death edict witness changed: '+name)
    for name, source in data['sources'].items():
        row = registry.get(name, {})
        if (not exact(source['oracle'], oracle.get(name)) or not exact(source['registry'], row) or
                row.get('kind') != 'card' or row.get('handler') in ('NULL','0','card_dummy')):
            raise ValueError('implemented normal death edict source changed: '+name)
        oracle_model(oracle[name])


class World(ModularWorld):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.forced_targets = 0
        self.active_player = self.controller

    def phase(self, p):
        from recipes import step

        self.active_player = p
        return [step('turn', player=p, value=0)]

    def is_creature(self, key):
        return 'Creature' in self.oracle[self.entities[key]['name']]['type_line'].split(' — ')[0].split()

    def cast_source(self, response=False, pending=False):
        from recipes import step

        p = self.controller
        run = self.fund(p, self.model['mana'])
        run += [step('cast_response' if response else 'cast', ref='source', value=1), step('assert_stack', value=2 if response else 1)]
        self.mana[p] = [0]*6
        self.zones['hand'][p].remove(self.name)
        self.source['alive'] = not pending
        if not (response or pending): run += [step('resolve'), step('assert_choices_consumed')]+self.effects()
        return run

    def deaths(self, keys, prefer_alternate=False):
        from recipes import step

        trigger = self.model['predicate']; watcher = self.source['alive']; p = self.controller
        if trigger == 'self':
            quantity = int(watcher and 'source' in keys)
        else:
            quantity = sum(watcher and self.entities[key]['player'] == p and self.is_creature(key) for key in keys)
        for key in keys:
            self.leave(key)
        order = [self.active_player, 1-self.active_player] if self.model['recipient'] == 'both' else [1-p]
        run = []
        for chooser in order:
            candidates = [key for key,e in self.entities.items() if e['alive'] and e['player'] == chooser and self.is_creature(key)]
            if prefer_alternate and chooser == 0 and 'alternative' in candidates:
                candidates.remove('alternative'); candidates.insert(0,'alternative')
            selected = candidates[:quantity]
            if chooser == 1 and selected and len(candidates) != len(selected):
                raise ValueError('AI sacrifice fixture must have a forced set of legal victims')
            for key in selected:
                if chooser == 0:
                    run += [step('choose_target', player=chooser, ref=self.entities[key]['ref'])]
                self.leave(key)
        return run

    def remove(self, key, removal='Murder', alternate=False):
        from recipes import step

        run = self.spell(removal, 'death_spell', (key,)); pending = len(run)
        if removal in ('Murder','Naturalize'):
            run += self.deaths([key], prefer_alternate=alternate)
        elif removal == 'Swords to Plowshares':
            e = self.entities[key]
            self.life[e['player']] += int(self.oracle[e['name']]['power'])
            self.leave(key, 'exile')
        else:
            self.leave(key,'hand')
        return run+[step('resolve'),step('assert_choices_consumed')]+self.effects(), pending

    def sweep(self, keys, sweeper):
        from recipes import step

        run = self.phase(self.controller)+self.spell(sweeper,'sweep',caster=self.controller)
        run += self.deaths(keys)+[step('resolve')]
        if sweeper == 'Planar Cleansing':
            # Measure the expected offer separately from the graveyard and battlefield.
            run += [step('assert_unused_choices', value=0)]
        else:
            run += [step('assert_choices_consumed')]
            damage = 6 if sweeper == 'Inferno' else 2
            if sweeper == 'Inferno':
                self.life = [life-damage for life in self.life]
            for entity in self.entities.values():
                if entity['alive'] and 'Creature' in self.oracle[entity['name']]['type_line']:
                    entity['damage'] += damage
        return run+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step, cost_from_oracle

    validate_witnesses(oracle, registry)
    source_pin = pins()['sources'].get(name, {})
    if source_pin.get('registry', {}).get('csvid') != csvid or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved source or unsupported death edict selection')
    result = []
    for variant in selected:
        mode, player = variant.rsplit('_',1)
        p = int(player); q = 1-p
        world = World(name,p,model,registry,oracle)
        gate = model['predicate'] == 'self'
        seeded = mode == 'stolen_source'
        world.add(name, 'source', p, 'battlefield' if seeded else 'hand', foreign=seeded)
        source_only = mode in ('payment','unaffordable','countered')
        dies = 'source' if gate or mode == 'source_dies' else 'victim'
        source_sweep = mode in ('simultaneous_source','simultaneous_damage')
        if not source_only:
            if dies == 'victim' or mode == 'other_death':
                victim_card = {'noncreature_death':'Howling Mine', 'sacrifice_cost_death':'Fume Spitter'}.get(mode,'Muck Rats')
                world.add(victim_card, 'victim', q if mode in ('other_death','lent_death') else p,
                          foreign=mode in ('stolen_death','lent_death'), token=mode=='token_death')
            if mode not in ('no_recipient_creatures', 'one_side_only'):
                if source_sweep: recipient = 'Darksteel Myr'
                else:
                    recipient = {'hexproof_recipient':'Slippery Bogle', 'indestructible_recipient':'Darksteel Myr',
                                 'flying_unblocked':'Runeclaw Bear','flying_reach':'Giant Spider'}.get(mode,'Craw Wurm')
                world.add(recipient,'recipient',q,foreign=mode=='stolen_recipient',token=mode=='token_recipient')
            if gate and mode not in ('no_recipient_creatures','self_sacrifice','other_death'):
                world.add('Darksteel Myr' if source_sweep else 'Runeclaw Bear','own_recipient',p)
            if (mode == 'simultaneous' or source_sweep) and not gate:
                world.add('Muck Rats','second_victim',p)
                world.add('Darksteel Myr' if source_sweep else 'Craw Wurm','second_recipient',q)
            if source_sweep and model['predicate'] == 'controlled_with_self':
                world.add('Darksteel Myr','third_recipient',q)
            if mode == 'no_recipient_creatures': world.add('Howling Mine','ineligible_recipient',q)
            if mode == 'chooser_alternate': world.add('Slippery Bogle','alternative',0)
            if mode == 'self_sacrifice': world.add("Ashnod's Altar", 'altar', p)
        run = []; ready_cut = pending_cut = None
        if mode == 'unaffordable':
            run += world.fund(p,[0]*6)+[step('cast',ref='source',value=0)]
        elif mode == 'countered':
            run += world.cast_source(pending=True)+[step('announce_spell',ref='source')]
            world.add('Counterspell','response',q,'hand')
            run += world.fund(q,cost_from_oracle(oracle['Counterspell']))+[
                step('cast_response',ref='response',value=1),step('resolve_response')]
            world.zones['hand'][q].remove('Counterspell'); world.zones['graveyard'][q].append('Counterspell'); world.mana[q] = [0]*6
            world.zones['graveyard'][p].append(name)
        elif mode == 'flash_before_death':
            run += world.phase(q)+world.spell('Murder','death_spell',('victim',))
            run += [step('announce_spell',ref='death_spell')]+world.cast_source(response=True)
            run += world.deaths(['victim'])+[step('resolve_response'),step('assert_choices_consumed')]+world.effects()
        elif mode == 'flash_after_death':
            run += world.phase(q)
            removal,_ = world.remove('victim'); run += removal+world.phase(p)+world.cast_source()
        else:
            if not seeded: run += world.cast_source()
            if mode == 'player_shroud':
                run += world.spell('Gilded Light','shield',caster=q)+[step('resolve'),step('assert_choices_consumed')]
            ready_cut = len(run)
            if mode == 'payment': pass
            elif mode == 'simultaneous' or source_sweep:
                source_dies = mode == 'simultaneous_source' or (mode == 'simultaneous_damage' and model['creature'])
                keys = (['source'] if source_dies else [])+([] if gate else ['victim','second_victim'])
                sweeper = {'simultaneous_source':'Planar Cleansing','simultaneous_damage':'Inferno'}.get(mode,'Pyroclasm')
                run += world.sweep(keys,sweeper)
            elif mode == 'self_sacrifice':
                if p == 0: run += [step('choose_target',player=p,ref='source')]
                run += world.deaths(['source']); world.mana[p][0] += 2
                run += [step('activate',ref='altar',value=1),step('resolve'),step('assert_choices_consumed')]+world.effects()
            elif mode == 'sacrifice_cost_death':
                # Fume Spitter itself pays the cost, which gives the watcher a real death.
                run += [step('choose_target',player=p,ref='recipient')]+world.deaths(['victim'])
                if world.entities['recipient']['alive']: world.entities['recipient']['minus'] += 1
                run += [step('activate',ref='victim',value=1),step('resolve'),step('assert_choices_consumed')]+world.effects()
            elif mode in ('flying_unblocked','flying_reach'):
                reach = mode == 'flying_reach'
                run += world.age()+[step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),
                    step('begin_blockers'),step('block',ref='recipient',other='source',value=int(reach)),
                    step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
                world.source['tapped'] = 1
                if reach:
                    world.source['damage'] = int(oracle['Giant Spider']['power']); world.leave('recipient')
                else:
                    world.life[q] -= int(oracle[name]['power'])
                run += world.effects()
                if not reach:
                    removal,_ = world.remove('source'); run += removal
            else:
                if mode == 'opposing_turn_death': run += world.phase(q)
                if mode == 'source_departed':
                    run += world.spell('Boomerang','source_departure',('source',))+[step('resolve')]
                    world.leave('source','hand')
                if mode == 'other_death': dies = 'victim'
                removal = {'bounce':'Boomerang','exile':'Swords to Plowshares','noncreature_death':'Naturalize'}.get(mode,'Murder')
                if mode == 'exile' and not world.is_creature(dies): raise ValueError('reviewed creature exile witness required')
                prefix, offset = world.remove(dies,removal,alternate=mode=='chooser_alternate')
                pending_cut = len(run)+offset; run += prefix
        checks = world.receipts()+world.effects()
        sequence = world.pre+run+checks
        if mode in ('ready_snapshot','pending_snapshot'):
            cut = ready_cut if mode == 'ready_snapshot' else pending_cut
            if cut is None: raise ValueError('real death edict replay boundary missing')
            tail = run[cut:]+checks
            sequence = (world.pre+run[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),
                        step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')])
        elif mode == 'resolved_snapshot':
            sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'DeathTriggeredEdictRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=world.setup,steps=sequence,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=GAPS)))
    return result
