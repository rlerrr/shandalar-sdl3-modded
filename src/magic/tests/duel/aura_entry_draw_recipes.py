"""Aura entry draw and the complete printed effects of the attached Aura."""
import copy
import json
from pathlib import Path

from death_counter_extended_recipes import World
from death_counter_trigger_recipes import exact

FAMILY = 'aura_entry_draw'
PROFILE = Path(__file__).parent / 'data/aura_entry_draw_wave8_profiles.json'
ENTRY = 'When this Aura enters, draw a card.'
EXTRAS = {
    ('Enchanted creature has "{R}: This creature gets +1/+0 until end of turn."',): 'red_pump',
    ('When enchanted creature dies, its controller draws a card.',): 'death_draw',
    ("Enchanted creature has intimidate. (It can't be blocked except by artifact creatures and/or creatures that share a color with it.)",): 'intimidate',
    ("Enchanted creature has flying. (It can't be blocked except by creatures with flying or reach.)",
     'Enchanted creature can block only creatures with flying.'): 'flying_defense_limit',
    ('Enchanted creature has protection from creatures.',): 'creature_protection',
    ('Enchanted creature gets +1/+1 and has flying.',): 'flying_stats',
}
MODES = {
    'red_pump': ('pump_cycle','pump_unfunded','opposing_payer_unfunded','opposing_payer_funded'),
    'death_draw': ('own_death','opposing_death','foreign_owned_death','removed_before_death','host_exiled'),
    'intimidate': ('intimidate_wrong_color','intimidate_same_color','intimidate_artifact'),
    'flying_defense_limit': ('flying_ground_blocker','flying_reach_blocker','defend_ground','defend_flying'),
    'creature_protection': ('protection_attack','protection_defend','protection_target'),
    'flying_stats': ('flying_ground_blocker','flying_reach_blocker'),
}
COMBAT = {
    'intimidate_wrong_color': ('Scathe Zombies', False, False),
    'intimidate_same_color': ('Runeclaw Bear', False, True),
    'intimidate_artifact': ('Wall of Tanglecord', False, True),
    'flying_ground_blocker': ('Runeclaw Bear', False, False),
    'flying_reach_blocker': ('Giant Spider', False, True),
    'defend_ground': ('Runeclaw Bear', True, False),
    'defend_flying': ('Wind Drake', True, True),
    'protection_attack': ('Runeclaw Bear', False, False),
    'protection_defend': ('Hill Giant', True, True),
}
HOST_REMOVAL = {
    **dict.fromkeys(('target_lost','host_bounced','foreign_owned_host'),('Unsummon','hand')),
    **dict.fromkeys(('own_death','opposing_death','foreign_owned_death','removed_before_death'),('Murder','graveyard')),
    'host_exiled': ('Scour from Existence','exile'),
}
AURA_REMOVAL = {'recast': ('Boomerang','hand'), 'aura_removed': ('Naturalize','graveyard'),
                'removed_before_death': ('Naturalize','graveyard')}
SCOPE_GAPS = ['independent response window between Aura attachment and mandatory entry draw',
              'arbitrary Aura movement and multiplayer']


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line') not in ('Enchantment — Aura','Enchantment — Aura Cartouche') or record.get('keywords')!=['Enchant']):
        raise ValueError('whole normal enchant creature Aura required')
    lines = record.get('oracle_text','').splitlines()
    if len(lines)<3 or lines[0] not in ('Enchant creature','Enchant creature you control') or lines[1]!=ENTRY:
        raise ValueError('complete enchant restriction and mandatory entry draw required')
    scope = 'controlled' if lines[0]=='Enchant creature you control' else 'any'
    extra = EXTRAS.get(tuple(lines[2:]))
    if not extra or (extra=='flying_stats')!=(scope=='controlled'):
        raise ValueError('unreviewed complete attached effect')
    return dict(schema=1,scope=scope,entry_draw=1,attached_effect=extra)


def spec(declaration, record):
    from recipes import cost_from_oracle

    expected = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),expected)):
        raise ValueError('complete typed Aura entry draw declaration required')
    model = dict(expected,family=FAMILY,mana=cost_from_oracle(record))
    selected = declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('distinct reviewed Aura entry draw variants required')
    return model


def variants(model):
    modes = ['entry','last_card','empty_library','insufficient_mana','countered','no_legal_host','target_lost',
             'host_bounced','aura_removed','recast','pending_snapshot','foreign_owned_host']
    if model['scope']=='any':
        modes.append('opposing_host')
    modes.extend(MODES[model['attached_effect']])
    return [f'{mode}_{p}' for p in (0,1) for mode in modes]


def validate_witnesses(oracle, registry):
    data = profiles()
    for name,pin in data['witnesses'].items():
        if not exact(pin['oracle'],oracle.get(name)) or not exact(pin['registry'],registry.get(name)):
            raise ValueError('Aura entry draw witness changed: '+name)
    for name,pin in data['sources'].items():
        row = registry.get(name,{})
        if (not exact(pin['oracle'],oracle.get(name)) or not exact(pin['registry'],row) or
                row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
            raise ValueError('implemented Aura entry draw source changed: '+name)
        parameters(oracle[name])


class AuraWorld(World):
    def __init__(self,name,controller,model,registry,oracle,mode):
        from recipes import card

        super().__init__(name,controller,model,registry,oracle)
        self.mode = mode
        self.effect = model['attached_effect']
        self.enemy = 1-controller
        opposing = mode in ('opposing_host','opposing_death','opposing_payer_unfunded','opposing_payer_funded')
        self.host_controller = self.enemy if opposing else controller
        self.attached = False
        if mode in ('empty_library','last_card'):
            self.setup = [item for item in self.setup if item['player']!=controller or item['zone']!='library']
            self.zones['library'][controller] = [] if mode=='empty_library' else ['Forest']
            if mode=='last_card':
                self.setup.append(card('Forest','last_library_card',controller,'library'))
        self.add(name,'source',controller,'hand')
        host = 'Grizzly Bears' if self.effect=='intimidate' else 'Hill Giant'
        if mode=='no_legal_host':
            if model['scope']=='controlled':
                self.host_controller = self.enemy
            else:
                host = 'Howling Mine'
        self.add(host,'host',self.host_controller,foreign=mode in ('foreign_owned_host','foreign_owned_death'))
        # Keep an independent alternate in each normal target-selection fixture.
        if mode!='no_legal_host':
            self.add('Eager Cadet','alternate',controller)
        if mode in COMBAT:
            self.add(COMBAT[mode][0],'combatant',1-self.host_controller)
        if mode=='protection_target':
            self.add('Prodigal Sorcerer','creature_source',self.enemy)
            self.add('Lightning Bolt','spell_source',self.enemy,'hand')
        # Every scripted card is physically seeded at the start. Include
        # unplayed helpers in every earlier checkpoint of the zone ledger.
        if mode in AURA_REMOVAL:
            self.add(AURA_REMOVAL[mode][0],'aura_removal',self.enemy,'hand')
        if mode in HOST_REMOVAL:
            self.add(HOST_REMOVAL[mode][0],'removal',1-self.host_controller,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',self.enemy,'hand')

    def draw(self,player):
        library = self.zones['library'][player]
        if library:
            self.zones['hand'][player].append(library.pop(0))
        else:
            # Normal draw_a_card() reaches the Shandalar actual loss callback,
            # observed through its -99 life sentinel.
            self.life[player] = -99

    def effects(self):
        return [receipt for key,e in self.entities.items() if e['alive'] for receipt in self.stats(key)]+self.mana_checks()

    def receipts(self):
        return super().receipts()+self.effects()

    def cast_source(self):
        from recipes import step

        p = self.controller
        mana = self.model['mana']
        if self.mode in ('insufficient_mana','no_legal_host'):
            enough = self.mode=='no_legal_host'
            return self.fund(p,mana if enough else [0]*6)+[
                step('require_can_cast',ref='source',value=int(not enough)),step('cast',ref='source',value=0),
                step('assert_unused_choices',value=0)]
        sequence = self.fund(p,mana)+[step('choose_target',player=p,ref='host'),step('cast',ref='source',value=1),
                   step('assert_stack',value=1),step('assert_unused_choices',value=0)]
        self.zones['hand'][p].remove(self.name)
        self.mana[p] = [0]*6
        # The current hand observer counts visible nonbattlefield instances,
        # including the Aura that the separate stack assertion sees pending.
        observed_hand = len(self.zones['hand'][p])+1
        return sequence+[step('assert_zone',player=p,zone='hand',value=observed_hand),
                         step('assert_zone',player=p,zone='library',value=len(self.zones['library'][p]))]

    def enter(self):
        from recipes import step

        self.entities['source'].update(alive=True,attached='host')
        self.attached = True
        if self.effect=='flying_stats':
            self.entities['host'].update(bp=1,bt=1)
        self.draw(self.controller)
        return [step('resolve'),step('assert_unused_choices',value=0)]+self.receipts()

    def cast_staged_instant(self,key,target=None,response=False):
        from recipes import cost_from_oracle, step

        e = self.entities[key]
        name, player = e['name'],e['player']
        run = self.fund(player,cost_from_oracle(self.oracle[name]))
        if target:
            run.append(step('choose_target',player=player,ref=target))
        run += [step('cast_response' if response else 'cast',ref=key,value=1),step('assert_stack',value=2 if response else 1)]
        self.zones['hand'][player].remove(name)
        self.zones['graveyard'][player].append(name)
        self.mana[player] = [0]*6
        return run

    def counter(self):
        from recipes import step

        run = [step('announce_spell',ref='source')]+self.cast_staged_instant('counter',response=True)
        run += [step('resolve_response'),step('assert_unused_choices',value=0)]
        self.zones['graveyard'][self.controller].append(self.name)
        return run

    def remove_host(self,pending=False):
        from recipes import step

        destination = HOST_REMOVAL[self.mode][1]
        run = [step('announce_spell',ref='source')] if pending else []
        run += self.cast_staged_instant('removal','host',response=pending)
        died_attached = self.attached and destination=='graveyard' and self.effect=='death_draw'
        self.leave('host',destination)
        if died_attached:
            self.draw(self.host_controller)
        self.attached = False
        run += [step('resolve_response' if pending else 'resolve'),step('assert_unused_choices',value=0)]
        if pending:
            self.zones['graveyard'][self.controller].append(self.name)
            run += [step('resolve'),step('assert_unused_choices',value=0)]
        return run

    def remove_aura(self):
        from recipes import step

        run = self.cast_staged_instant('aura_removal','source')
        self.leave('source',AURA_REMOVAL[self.mode][1])
        self.attached = False
        if self.effect=='flying_stats':
            self.entities['host'].update(bp=0,bt=0)
        return run+[step('resolve'),step('assert_unused_choices',value=0)]+self.receipts()

    def pump(self):
        from recipes import observe, step

        owner, host = self.controller,self.host_controller
        if self.mode.startswith('opposing_payer'):
            funded = self.mode=='opposing_payer_funded'
            player = host if funded else owner
            return self.fund(player,[0,0,0,0,1,0])+[observe('source','in_play',1),
                    step('can_activate',ref='source',value=int(funded))]
        if self.mode=='pump_unfunded':
            return [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),
                    step('assert_unused_choices',value=0)]
        run = self.fund(host,[0,0,0,0,2,0])
        for remaining in (1,0):
            run += [step('require_can_activate',ref='source',value=1),step('activate',ref='source',value=1),
                    step('assert_stack',value=1),step('assert_unused_choices',value=0),
                    step('assert_mana',player=host,color=4,value=remaining),step('resolve')]
            self.mana[host][4] = remaining
            self.entities['host']['bp'] += 1
            run += self.effects()
        run += self.combat('host',None,False)
        self.entities['host']['bp'] = 0
        return run+[step('cleanup')]+self.effects()

    def combat(self, attacker, blocker, legal):
        from recipes import observe, step

        a = self.entities[attacker]
        actor = a['player']
        power = int(self.oracle[a['name']]['power'])+a['bp']
        run = [step('turn',player=actor,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),
               step('finish_attackers'),step('begin_blockers')]
        a['tapped'] = 1
        if blocker:
            run += [step('block',ref=blocker,other=attacker,value=int(legal)),observe(blocker,'blocking_count',int(legal))]
        if legal:
            b = self.entities[blocker]
            b['damage'] = 0 if self.effect=='creature_protection' and blocker=='host' else power
            a['damage'] = int(self.oracle[b['name']]['power'])+b['bp']
            for key in (attacker,blocker):
                e = self.entities[key]
                if e['damage']>=int(self.oracle[e['name']]['toughness'])+e['bt']:
                    self.leave(key)
        else:
            self.life[1-actor] -= power
        return run+[step('advance_combat'),observe(attacker,'blocked',int(legal)),step('combat_damage'),
                    step('resolve'),step('assert_unused_choices',value=0),step('end_combat')]+self.effects()


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    pin = profiles()['sources'].get(name,{})
    if pin.get('registry',{}).get('csvid')!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('unreserved Aura entry draw source or variant')
    result = []
    for variant in selected:
        mode,player = variant.rsplit('_',1)
        w = AuraWorld(name,int(player),model,registry,oracle,mode)
        actions = w.cast_source()
        pending = len(actions)
        if mode=='countered':
            actions += w.counter()
        elif mode=='target_lost':
            actions += w.remove_host(pending=True)
        elif mode not in ('no_legal_host','insufficient_mana'):
            actions += w.enter()
            if mode=='recast':
                actions += w.remove_aura()+[step('bind_hand_copy',player=w.controller,value=csvid,index=0,ref='source')]
                actions += w.cast_source()+w.enter()
            elif mode in AURA_REMOVAL:
                actions += w.remove_aura()
            if mode in HOST_REMOVAL:
                actions += w.remove_host()
            elif mode in MODES['red_pump']:
                actions += w.pump()
            elif mode in COMBAT:
                _,defend,legal = COMBAT[mode]
                attacker,blocker = ('combatant','host') if defend else ('host','combatant')
                actions += w.combat(attacker,blocker,legal)
            elif mode=='protection_target':
                actions += [step('can_target_creature',ref='creature_source',other='host',value=0),
                            step('can_target_creature',ref='creature_source',other='alternate',value=1),
                            step('can_target_creature',ref='spell_source',other='host',value=1)]
        actions += w.receipts()
        if mode=='pending_snapshot':
            replay = actions[pending:]
            actions = actions[:pending]+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),
                       step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        gaps = SCOPE_GAPS+(['independent opposing-provider activation UI; the real cross-controller payer query is covered']
                           if model['attached_effect']=='red_pump' else [])
        result.append(dict(name=f'AuraEntryDrawRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=w.setup,steps=w.pre+actions,_recipe=dict(family=FAMILY,version=2,variant=variant,
                           params=declaration['params'],oracle_text=declaration['oracle_text'],scope_gaps=gaps)))
    return result
