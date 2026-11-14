"""Ordinary equip costs, permanent stat bonuses and unconditional keywords."""
import re
from collections import Counter
from creature_recipes import REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

KEYWORDS=('Flying','Reach','Trample','First strike','Double strike','Deathtouch','Lifelink',
          'Haste','Vigilance','Hexproof','Shroud','Indestructible','Menace')
EQUIP_REMINDER='({cost}: Attach to target creature you control. Equip only as a sorcery.)'
EXTRA_REMINDERS={
    "(Attacking doesn't cause it to tap.)", "(It can block creatures with flying.)",
    "(It deals both first-strike and regular combat damage.)",
    "(It can't be the target of spells or abilities.)",
    "(It can't be the target of spells or abilities your opponents control.)",
    "(It can't be the target of spells or abilities your opponents control. It can attack and {T} no matter when it came under your control.)",
    "(Any amount of damage it deals to a creature is enough to destroy it. Damage dealt by this creature also causes you to gain that much life.)",
    "(Any amount of damage it deals to a creature is enough to destroy that creature.)",
    "(It can deal excess combat damage to the player or planeswalker it's attacking.)",
    "(It can't be blocked except by two or more creatures.)",
    "(Effects that say \"destroy\" don't destroy this Equipment.)",
}
SPELLS={
    'Boomerang':('{U}{U}',"Return target permanent to its owner's hand."),
    'Unsummon':('{U}',"Return target creature to its owner's hand."),
    'Suppression Field':('{1}{W}',"Activated abilities cost {2} more to activate unless they're mana abilities."),
}
WITNESSES={'Grizzly Bears':('2','2',''), 'Hill Giant':('3','3',''), 'Scaled Wurm':('7','6',''),
           'Suntail Hawk':('1','1','Flying'), 'Giant Spider':('2','4','Reach (This creature can block creatures with flying.)'),
           'Llanowar Elves':('1','1','{T}: Add {G}.')}

def spec(declaration,record):
    if isinstance(declaration.get('params'),dict) and declaration['params'].get('schema')==7:
        return wave7_spec(declaration,record)
    from recipes import cost_from_oracle
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Artifact' not in record.get('type_line','') or 'Equipment' not in record.get('type_line','')
            or 'Creature' in record.get('type_line','') or declaration.get('oracle_text')!=record.get('oracle_text')
            or 'variants' not in declaration):raise ValueError('equipment requires pinned normal noncreature Equipment and reviewed variants')
    p=declaration.get('params')
    if not isinstance(p,dict) or set(p)!={'power','toughness','keywords','equip_cost','indestructible'}:
        raise ValueError('explicit equipment stats, keywords, equip cost and indestructibility required')
    if (type(p['power']) is not int or not 0<=p['power']<=6 or type(p['toughness']) is not int or not -1<=p['toughness']<=6
            or type(p['indestructible']) is not bool):raise ValueError('bounded signed equipment stats and boolean indestructibility required')
    keys=p['keywords'];cost=p['equip_cost']
    if not isinstance(keys,list) or any(k not in KEYWORDS for k in keys) or keys!=sorted(set(keys)):
        raise ValueError('explicit sorted unique unconditional equipment keywords required')
    if not isinstance(cost,str) or not re.fullmatch(r'\{[0-7]\}',cost):raise ValueError('reviewed zero-to-seven generic equip cost required')
    clauses=[]
    for raw in record['oracle_text'].splitlines():
        text=raw
        for reminder in re.findall(r'\([^\n]*?\)',raw):
            if reminder not in REMINDERS|EXTRA_REMINDERS|{EQUIP_REMINDER.format(cost=cost)}:
                raise ValueError('unreviewed equipment reminder')
            text=text.replace(reminder,'')
        clauses.append(text.strip())
    equip='Equip '+cost
    if clauses.count(equip)!=1 or clauses.count('Indestructible')!=int(p['indestructible']):
        raise ValueError('equip cost and source indestructibility must match whole Oracle text')
    effects=[line for line in clauses if line not in (equip,'Indestructible')]
    if len(effects)!=1:raise ValueError('additional equipment abilities require dedicated cases')
    line=effects[0]
    stats=f"Equipped creature gets {p['power']:+d}/{p['toughness']:+d}"
    if line.startswith(stats):
        suffix=line[len(stats):]
        if suffix=='.':found=[]
        elif suffix.startswith(' and has ') and suffix.endswith('.'):found=suffix[len(' and has '):-1]
        else:raise ValueError('equipment stat or additional effect mismatch')
    elif p['power']==p['toughness']==0 and line.startswith('Equipped creature has ') and line.endswith('.'):
        found=line[len('Equipped creature has '):-1]
    else:raise ValueError('equipment effect differs from explicit stats')
    if isinstance(found,str):
        found=[s.strip().capitalize() for s in re.split(r', and |, | and ',found)]
        canonical={k.lower():k for k in KEYWORDS}
        if any(k.lower() not in canonical for k in found):raise ValueError('unreviewed equipped keyword or extra clause')
        found=[canonical[k.lower()] for k in found]
    if sorted(found)!=keys:raise ValueError('equipped keywords differ from explicit parameters')
    # Oracle keyword tags describe the Equipment itself, not granted abilities.
    expected={'Equip'}|({'Indestructible'} if p['indestructible'] else set())
    if set(record.get('keywords',[]))!=expected:raise ValueError('unreviewed structured equipment keyword')
    mana=[0]*6 if record.get('mana_cost')=='{0}' else cost_from_oracle(record)
    return dict(p,family='equipment',mana=mana,equip_mana=[int(cost[1]),0,0,0,0,0])

def variants(p):
    if p.get('schema')==7:return wave7_variants(p)
    modes=['attach','cast','cast_payment','equip_payment','red_payment','move','tapped_source','tapped_host','sick_host',
           'empty','wrong_controller','wrong_type','cleanup','equipment_bounce','equipment_destroy','host_destroy',
           'lost_target','move_lost_target','source_bounced_pending','ready_snapshot','pending_snapshot','resolved_snapshot',
           'bare_zones','unblocked','fresh_attack','tax']
    keys=set(p['keywords'])
    if 'Shroud' not in keys:modes+=['same_target']
    if not keys&{'Shroud','Hexproof'}:modes+=['host_bounce']
    if p['equip_mana'][0]:modes+=['insufficient_equip_mana']
    if sum(p['mana']):modes+=['insufficient_cast_mana']
    if 'Flying' in keys:modes+=['flying_evasion','removed_flying']
    if keys&{'Flying','Reach'}:modes+=['block_flying','removed_block_flying']
    if keys&{'First strike','Double strike'}:modes+=['first_strike']
    if 'Deathtouch' in keys:modes+=['deathtouch']
    if 'Trample' in keys:modes+=['trample']
    if keys&{'Hexproof','Shroud'}:modes+=['owner_targeting','opponent_targeting','removed_targeting']
    if 'Indestructible' in keys:modes+=['host_indestructible_removed']
    if 'Menace' in keys:modes+=['menace_single','menace_pair']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def validate_witnesses(oracle,registry):
    for name,fields in SPELLS.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=fields:raise ValueError('equipment spell witness changed: '+name)
    for name,fields in WITNESSES.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=fields:
            raise ValueError('equipment creature witness changed: '+name)

def expand(name,csvid,declaration,p,selected,registry,oracle):
    if p.get('schema')==7:return wave7_expand(name,csvid,declaration,p,selected,registry,oracle)
    from recipes import card,step,observe
    result=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);keys=set(p['keywords'])
        empty=v=='bare_zones';fixture='empty_main' if empty else 'stocked_zones'
        hands=[list(() if empty else HAND) for _ in (0,1)];graves=[list(() if empty else GRAVE) for _ in (0,1)]
        libs=[list(() if empty else LIBRARY) for _ in (0,1)];exiles=[list(() if empty else EXILE) for _ in (0,1)]
        mana=[[10]*6 for _ in (0,1)];life=[20,20];setup=[];entities={};pre=[];actions=[];follow=[]
        casting=v in ('cast','cast_payment','insufficient_cast_mana')
        def add(nm,ref,player,zone='battlefield',state=None):
            c=card(nm,ref,player,zone)
            if state:c['state']=state
            setup.append(c)
            if zone=='hand':hands[player].append(nm)
            else:entities[ref]=dict(name=nm,player=player,alive=True,power=int(oracle[nm].get('power',0)),toughness=int(oracle[nm].get('toughness',0)))
        add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped_source' else None)
        if casting:entities['source']=dict(name=name,player=owner,alive=False,power=0,toughness=0)
        negative=v in ('empty','wrong_controller','wrong_type','insufficient_equip_mana','insufficient_cast_mana')
        host_owner=1-owner if v=='wrong_controller' else owner
        if v!='empty':add('Sol Ring' if v=='wrong_type' else 'Grizzly Bears','host',host_owner,state=['tapped'] if v=='tapped_host' else ['sick'] if v in ('sick_host','fresh_attack') else None)
        probe=v in ('owner_targeting','opponent_targeting','removed_targeting')
        if not negative and not probe:add('Hill Giant','other',owner);add('Hill Giant','enemy_control',1-owner)
        if v in ('flying_evasion','removed_flying','menace_single','menace_pair'):add('Grizzly Bears','blocker',1-owner)
        if v=='menace_pair':add('Grizzly Bears','blocker2',1-owner)
        if v=='trample':add('Suntail Hawk','blocker',1-owner)
        if v in ('block_flying','removed_block_flying'):add('Suntail Hawk','attacker',1-owner)
        if v=='first_strike':add('Llanowar Elves','attacker',1-owner)
        if v=='deathtouch':add('Scaled Wurm','attacker',1-owner)
        pre += [step('turn',player=owner,value=0)]
        if v=='tax':
            add('Suppression Field','field',1-owner,'hand')
            pre += [step('turn',player=1-owner,value=0),step('cast',ref='field',value=1),step('resolve'),step('assert_choices_consumed'),step('turn',player=owner,value=0)]
            hands[1-owner].remove('Suppression Field');entities['field']=dict(name='Suppression Field',player=1-owner,alive=True,power=0,toughness=0)
        if v in ('equip_payment','red_payment'):mana[owner]=list(p['equip_mana'])
        if v=='red_payment':mana[owner]=[0,0,0,0,p['equip_mana'][0],0]
        if v=='tax':mana[owner]=[p['equip_mana'][0]+2,0,0,0,0,0]
        if v=='cast_payment':mana[owner]=[a+b for a,b in zip(p['mana'],p['equip_mana'])]
        if v in ('insufficient_equip_mana','insufficient_cast_mana'):mana[owner]=[0]*6
        pre += [step('set_mana',player=player,color=c,value=n) for player in (0,1) for c,n in enumerate(mana[player])]+[step('refresh')]
        equipped=None
        def stats(ref,boosted):
            e=entities[ref]
            return [step('require_card',ref=ref,property=k,value=e[k]+(p[k] if boosted else 0)) for k in ('power','toughness')]
        def equip(ref='host',resolve=True):
            cost=p['equip_mana'][0]+(2 if v=='tax' else 0)
            seq=[step('require_can_activate',ref='source',value=1)]
            if v=='red_payment' and owner==0:seq += [step('choose_mana',player=owner,color=4) for _ in range(cost)]
            seq += [step('choose_target',player=owner,ref=ref),step('activate',ref='source',value=1),step('assert_stack',value=1),step('assert_choices_consumed')]
            mana[owner][4 if v=='red_payment' else 0]-=cost
            if resolve:seq += [step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')]+stats(ref,True)
            return seq
        if casting:
            if v=='insufficient_cast_mana':actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
            else:
                actions += [step('cast',ref='source',value=1),step('assert_stack',value=1)]+stats('host',False)+[step('resolve'),step('refresh'),step('require_card',ref='source',property='in_play',value=1)]
                hands[owner].remove(name);entities['source']['alive']=True;mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])]
        if negative:
            if v!='insufficient_cast_mana':
                actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')]
        else:
            ready=len(actions)
            actions += equip(resolve=False);pending=len(actions)
            actions += stats('host',False)
            if v=='lost_target':
                add('Unsummon','response',0,'hand');actions += [step('choose_target',player=0,ref='host'),step('cast',ref='response',value=1),step('resolve'),step('refresh')]
                hands[0].remove('Unsummon');graves[0].append('Unsummon');hands[owner].append('Grizzly Bears');entities['host']['alive']=False;mana[0][2]-=1
            elif v=='source_bounced_pending':
                add('Boomerang','response',0,'hand');actions += [step('choose_target',player=0,ref='source'),step('cast',ref='response',value=1),step('resolve'),step('refresh')]
                hands[0].remove('Boomerang');graves[0].append('Boomerang');hands[owner].append(name);entities['source']['alive']=False;mana[0][2]-=2
            else:
                actions += [step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')]+stats('host',True);equipped='host'
            if v=='source_bounced_pending':actions += stats('host',False)
            if v in ('move','same_target','move_lost_target'):
                target='host' if v=='same_target' else 'other'
                if v=='move_lost_target':
                    actions += equip(target,False)
                    add('Unsummon','response',0,'hand');actions += [step('choose_target',player=0,ref=target),step('cast',ref='response',value=1),step('resolve'),step('refresh')]+stats('host',True)
                    hands[0].remove('Unsummon');graves[0].append('Unsummon');hands[owner].append('Hill Giant');entities[target]['alive']=False;mana[0][2]-=1
                else:
                    actions += equip(target);equipped=target
                    if v=='move':actions += stats('host',False)
            removed=v in ('equipment_bounce','removed_flying','removed_block_flying','removed_targeting','host_indestructible_removed')
            if removed:
                add('Boomerang','removal',0,'hand');follow += [step('choose_target',player=0,ref='source'),step('cast',ref='removal',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]+stats('host',False)
                hands[0].remove('Boomerang');graves[0].append('Boomerang');hands[owner].append(name);entities['source']['alive']=False;mana[0][2]-=2;equipped=None
            if v=='equipment_destroy':
                follow += [step('destroy',ref='source'),step('refresh')]
                if not p['indestructible']:entities['source']['alive']=False;graves[owner].append(name);equipped=None
                follow += stats('host',bool(equipped))
            if v in ('host_destroy','host_indestructible_removed'):
                survives='Indestructible' in keys and not removed
                follow += [step('destroy',ref='host'),step('refresh'),observe('host','in_play',int(survives)),observe('source','in_play',int(not removed))]
                if not survives:entities['host']['alive']=False;graves[owner].append('Grizzly Bears');equipped=None
            if v=='host_bounce':
                add('Unsummon','removal',0,'hand');follow += [step('choose_target',player=0,ref='host'),step('cast',ref='removal',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),observe('source','in_play',1)]
                hands[0].remove('Unsummon');graves[0].append('Unsummon');hands[owner].append('Grizzly Bears');entities['host']['alive']=False;mana[0][2]-=1;equipped=None
            if v=='cleanup':follow += [step('cleanup'),step('refresh')]+stats('host',True)
            effective=set() if removed else keys
            if probe:
                actor=owner if v=='owner_targeting' else 1-owner
                add('Unsummon','probe',actor,'hand')
                legal=not effective&{'Shroud'} and (actor==owner or 'Hexproof' not in effective)
                # AI can decline a legal Unsummon of its own creature. Query the
                # normal creature-target validator rather than strategic desire.
                follow.append(step('can_target_creature',ref='probe',other='host',value=int(legal)))
            attacking=v in ('unblocked','fresh_attack','flying_evasion','removed_flying','trample','menace_single','menace_pair')
            if attacking:
                legal=v!='fresh_attack' or 'Haste' in effective
                follow += [step('begin_attackers'),step('attack',ref='host',value=int(legal)),observe('host','attacking',int(legal)),observe('host','tapped',int(legal and 'Vigilance' not in effective)),step('finish_attackers'),step('begin_blockers')]
                blocking=v in ('removed_flying','trample','menace_single','menace_pair')
                if 'blocker' in entities:
                    follow += [step('block',ref='blocker',other='host',value=int(blocking)),observe('blocker','blocking_count',int(blocking))]
                if v=='menace_pair':follow.append(step('block',ref='blocker2',other='host',value=1))
                follow.append(step('advance_combat'))
                if v=='menace_single':follow += [observe('host','blocked',0),observe('blocker','blocking_count',0)];blocking=False
                if v in ('removed_flying','menace_pair'):pass # Legality only; damage ordering needs its own scenario.
                else:
                    follow.append(step('combat_damage'))
                    power=2+(p['power'] if equipped else 0);total=power*(2 if 'Double strike' in effective else 1) if legal else 0
                    dealt=max(0,total-1) if v=='trample' else total
                    life[1-owner]-=dealt
                    if 'Lifelink' in effective:life[owner]+=total
                    if v=='trample':entities['blocker']['alive']=False;graves[1-owner].append('Suntail Hawk')
            if 'attacker' in entities:
                follow += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers')]
                legal=v!='removed_block_flying'
                follow += [step('block',ref='host',other='attacker',value=int(legal)),observe('host','blocking_count',int(legal)),step('advance_combat')]
                if v in ('first_strike','deathtouch'):
                    follow.append(step('combat_damage'))
                    first=bool(effective&{'First strike','Double strike'});power=2+p['power'];toughness=2+p['toughness']
                    damage=0 if first else 7 if v=='deathtouch' else 1
                    dead=damage>=toughness and 'Indestructible' not in effective
                    entities['attacker']['alive']=False;graves[1-owner].append(entities['attacker']['name'])
                    follow += [observe('attacker','in_play',0),observe('host','in_play',int(not dead))]
                    if dead:entities['host']['alive']=False;graves[owner].append('Grizzly Bears');equipped=None
                    else:follow.append(observe('host','damage',damage))
                    if 'Lifelink' in effective:life[owner]+=power
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            checks.append(observe(ref,'in_play',int(e['alive'])))
            if e['alive'] and e['name'] in WITNESSES:checks += stats(ref,ref==equipped)
        if v in ('tapped_source','tapped_host'):checks.append(observe('source' if v=='tapped_source' else 'host','tapped',1))
        # Haste can change the engine's sickness bits. This case proves that
        # sickness does not prevent equipping; attack permission has its own case.
        if v=='sick_host':checks.append(observe('host','tapped',0))
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
            checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exiles[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
        steps=pre+actions+follow+checks
        if v in ('ready_snapshot','pending_snapshot'):
            cut=ready if v=='ready_snapshot' else pending
            steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+follow+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+follow+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('destroy',ref='other'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'EquipmentRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='equipment',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=['equip priority permissions','multiplayer','damage ordering for multiple blockers'])))
    return result


# Count-derived bonuses use an isolated grammar and model. Legacy fixed bonuses
# retain their original scenarios and parameters above.
WAVE7_PROFILES={
    'Empyrial Plate':('hand','Equipped creature gets +1/+1 for each card in your hand.','{2}'),
    'Pennon Blade':('creatures','Equipped creature gets +1/+1 for each creature you control.','{4}'),
    'Helm of the Gods':('enchantments','Equipped creature gets +1/+1 for each enchantment you control.','{1}'),
    'Civic Saber':('colors','Equipped creature gets +1/+0 for each of its colors.','{1}'),
    'Hedron Matrix':('mana_value','Equipped creature gets +X/+X, where X is its mana value.','{4}'),
    'Golem-Skin Gauntlets':('attachments','Equipped creature gets +1/+0 for each Equipment attached to it.','{2}'),
}

def wave7_data():
    import json
    from pathlib import Path
    return json.loads((Path(__file__).parent/'data/equipment_wave7_witnesses.json').read_text(encoding='utf8'))

def wave7_spec(declaration,record):
    from recipes import cost_from_oracle
    if declaration.get('family')!='equipment' or type(declaration.get('version')) is not int or declaration['version']!=2:
        raise ValueError('count-derived Equipment requires family equipment and integer version 2')
    p=declaration['params']
    if set(p)!={'schema','profile'} or type(p['schema']) is not int or p['schema']!=7:
        raise ValueError('equipment schema 7 requires only its reviewed count profile')
    name=(record or {}).get('name')
    if name not in WAVE7_PROFILES:raise ValueError('unreviewed count-derived Equipment')
    profile,clause,cost=WAVE7_PROFILES[name]
    pinned=wave7_data()['sources'][name]
    if (p['profile']!=profile or declaration.get('oracle_text')!=record.get('oracle_text')
            or 'variants' not in declaration or any(record.get(k)!=v for k,v in pinned['oracle'].items())
            or record.get('card_faces') or record.get('selected_face') or record.get('layout')!='normal'
            or 'Artifact' not in record.get('type_line','') or 'Equipment' not in record.get('type_line','')
            or 'Creature' in record.get('type_line','') or set(record.get('keywords',[]))!={'Equip'}):
        raise ValueError('count-derived Equipment identity, complete Oracle record or profile changed')
    lines=[]
    for raw in record['oracle_text'].splitlines():
        text=raw
        for reminder in re.findall(r'\([^\n]*?\)',raw):
            if reminder!=EQUIP_REMINDER.format(cost=cost):raise ValueError('unreviewed count Equipment reminder')
            text=text.replace(reminder,'')
        lines.append(text.strip())
    if lines!=[clause,'Equip '+cost]:raise ValueError('count-derived Equipment must match its whole rules text')
    result=dict(schema=7,family='equipment',profile=profile,mana=cost_from_oracle(record),equip_mana=int(cost[1]),
                power_scale=1,toughness_scale=int(profile not in ('colors','attachments')))
    selected=declaration['variants']
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected))!=len(selected) or not set(selected)<=set(wave7_variants(result))):
        raise ValueError('count-derived Equipment requires explicit distinct supported string variants')
    return result

def wave7_variants(p):
    modes=['cast_attach','exact_payment','generic_red','move','cleanup','source_bounce','host_bounce',
           'source_destroy','host_destroy','ready_snapshot','pending_snapshot','resolved_snapshot',
           'empty','wrong_controller','wrong_type','no_equip_mana','opponent_turn','tapped_source',
           'tapped_host','sick_host','unblocked','bare_zones','lost_target','source_bounced_pending']
    if p['profile'] in ('hand','creatures','enchantments','attachments'):modes+=['increase','decrease']
    if p['profile'] in ('colors','mana_value'):modes+=['zero_host','many_host']
    if p['profile']=='enchantments':modes+=['zero_count']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def wave7_validate(oracle,registry):
    for name,pinned in wave7_data()['witnesses'].items():
        if registry.get(name,{}).get('csvid')!=pinned['csvid'] or any(oracle.get(name,{}).get(k)!=v for k,v in pinned['oracle'].items()):
            raise ValueError('count-derived Equipment witness changed: '+name)

def wave7_expand(name,csvid,declaration,p,selected,registry,oracle):
    import copy
    from recipes import card,step,cost_from_oracle
    wave7_validate(oracle,registry)
    if registry[name]['csvid']!=wave7_data()['sources'][name]['csvid'] or csvid!=registry[name]['csvid']:
        raise ValueError('count-derived Equipment registry identity changed')
    result=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);profile=p['profile']
        bare=mode=='bare_zones'
        zones=[dict(hand=list(() if bare else HAND),graveyard=list(() if bare else GRAVE),
                    library=list(() if bare else LIBRARY),exile=list(() if bare else EXILE)) for _ in (0,1)]
        setup=[];entities={};attached={};steps=[]
        negative=mode in ('empty','wrong_controller','wrong_type','no_equip_mana','opponent_turn')
        seeded=negative or mode in ('generic_red','tapped_source')
        pools=[[40,10,10,10,10,10] for _ in (0,1)]
        if mode=='exact_payment':pools=[[0]*6 for _ in (0,1)];pools[owner][0]=sum(p['mana'])+p['equip_mana']+int(profile=='attachments')
        if mode=='no_equip_mana':pools[owner]=[0]*6
        if mode=='generic_red':pools[owner]=[0,0,0,0,p['equip_mana']+int(profile=='attachments'),0]
        totals=[sum(pool) for pool in pools]
        def add(nm,ref,who,zone='battlefield',state=None):
            c=card(nm,ref,who,zone)
            if state:c['state']=state
            setup.append(c);entities[ref]=dict(name=nm,player=who,alive=zone=='battlefield')
            if zone!='battlefield':zones[who][zone].append(nm)
        def creature(ref):return 'Creature' in oracle[entities[ref]['name']].get('type_line','')
        source_state=['tapped'] if mode=='tapped_source' else None
        add(name,'source',owner,'battlefield' if seeded else 'hand',source_state)
        host_name='Ornithopter' if mode=='zero_host' else ('Scaled Wurm' if profile=='mana_value' else 'Watchwolf') if mode=='many_host' else 'Grizzly Bears'
        if mode!='empty':add('Sol Ring' if mode=='wrong_type' else host_name,'host',1-owner if mode=='wrong_controller' else owner,
                             state=['tapped'] if mode=='tapped_host' else ['sick'] if mode=='sick_host' else None)
        if not negative:
            add('Hill Giant','other',owner);add('Grizzly Bears','enemy',1-owner)
            # Populated opposing boards and irrelevant types expose count scope.
            if profile=='creatures':add('Sol Ring','noncreature',owner);add('Hill Giant','enemy_extra',1-owner)
            if profile=='enchantments':
                if mode!='zero_count':add('Seal of Cleansing','counted',owner)
                add('Seal of Cleansing','enemy_enchantment',1-owner);add('Howling Mine','nonenchantment',owner)
            if profile=='attachments':add('Bonesplitter','other_blade',owner)
            if mode in ('source_bounce','host_bounce','lost_target','source_bounced_pending'):
                add('Boomerang' if mode in ('source_bounce','source_bounced_pending') else 'Unsummon','removal',0,'hand')
            if mode=='increase':
                addition={'hand':'Divination','creatures':'Ornithopter','enchantments':'Seal of Cleansing','attachments':'Bonesplitter'}[profile]
                add(addition,'addition',owner,'hand')
            if mode=='decrease' and profile=='hand':add('Sol Ring','addition',owner,'hand')
            if mode=='decrease' and profile=='attachments':add('Bonesplitter','counted',owner)
        steps += [step('turn',player=1-owner if mode=='opponent_turn' else owner,value=0)]
        steps += [step('set_mana',player=who,color=color,value=amount) for who in (0,1) for color,amount in enumerate(pools[who])]
        steps.append(step('refresh'))
        def amount(ref):
            if profile=='hand':return len(zones[owner]['hand'])
            if profile in ('creatures','enchantments'):
                kind='Creature' if profile=='creatures' else 'Enchantment'
                return sum(e['alive'] and e['player']==owner and kind in oracle[e['name']].get('type_line','') for e in entities.values())
            if profile=='colors':return len(oracle[entities[ref]['name']]['colors'])
            if profile=='mana_value':return int(oracle[entities[ref]['name']]['cmc'])
            return sum(e['alive'] and attached.get(r)==ref and 'Equipment' in oracle[e['name']].get('type_line','') for r,e in entities.items())
        def stats():
            seq=[]
            for ref,e in entities.items():
                if not e['alive'] or not creature(ref):continue
                base=oracle[e['name']];power=int(base['power']);toughness=int(base['toughness'])
                for equipment,target in attached.items():
                    if target!=ref or not entities[equipment]['alive']:continue
                    if equipment=='source':power+=amount(ref)*p['power_scale'];toughness+=amount(ref)*p['toughness_scale']
                    elif entities[equipment]['name']=='Bonesplitter':power+=2
                seq += [step('require_card',ref=ref,property='power',value=power),step('require_card',ref=ref,property='toughness',value=toughness)]
            return seq
        def cast(ref,target=None):
            e=entities[ref];nm=e['name'];who=e['player'];cost=[0]*6 if oracle[nm].get('mana_cost')=='{0}' else cost_from_oracle(oracle[nm])
            if target:steps.append(step('choose_target',player=who,ref=target))
            steps.extend([step('cast',ref=ref,value=1),step('assert_stack',value=1),step('assert_choices_consumed'),step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
            totals[who]-=sum(cost);zones[who]['hand'].remove(nm)
            if 'Instant' in oracle[nm]['type_line'] or 'Sorcery' in oracle[nm]['type_line']:zones[who]['graveyard'].append(nm)
            else:e['alive']=True
        def activate(ref,target):
            who=entities[ref]['player'];cost=p['equip_mana'] if ref=='source' else 1
            steps.append(step('require_can_activate',ref=ref,value=1))
            if mode=='generic_red' and who==0:steps.extend(step('choose_mana',player=who,color=4) for _ in range(cost))
            steps.extend([step('choose_target',player=who,ref=target),step('activate',ref=ref,value=1),step('assert_stack',value=1),step('assert_choices_consumed')])
            totals[who]-=cost
        def resolve_equipment(ref,target):
            steps.extend([step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')]);attached[ref]=target
        def destroy(ref):
            e=entities[ref];steps.extend([step('destroy',ref=ref),step('refresh')]);e['alive']=False;zones[e['player']]['graveyard'].append(e['name']);attached.pop(ref,None)
            for equipment,target in list(attached.items()):
                if target==ref:attached.pop(equipment)
        def checks():
            seq=[step('assert_stack',value=0),step('assert_choices_consumed')]+stats()
            seq += [step('require_card',ref=ref,property='in_play',value=int(e['alive'])) for ref,e in entities.items()]
            for who in (0,1):
                seq.extend([step('assert_life',player=who,value=20),step('assert_mana_total',player=who,value=totals[who]),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))])
                for zone,names in zones[who].items():
                    seq.append(step('assert_zone',player=who,zone=zone,value=len(names)))
                    if zone=='library':seq.extend(step('assert_zone_card',player=who,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names))
                    else:
                        for nm,count in Counter(names).items():seq.append(step('assert_zone_multiplicity',player=who,zone=zone,index=registry[nm]['csvid'],value=count))
            return seq
        if negative:
            steps.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_choices_consumed')])
        else:
            if not seeded:cast('source')
            if profile=='attachments':activate('other_blade','other');resolve_equipment('other_blade','other')
            if mode=='decrease' and profile=='attachments':activate('counted','host');resolve_equipment('counted','host')
            ready=len(steps)
            steps.extend(stats());activate('source','host');pending=len(steps)
            # Costs and targeting are paid before attachment changes stats.
            steps.extend(stats())
            if mode in ('lost_target','source_bounced_pending'):
                target='host' if mode=='lost_target' else 'source';removal=entities['removal']['name']
                steps.extend([step('choose_target',player=0,ref=target),step('cast',ref='removal',value=1),step('assert_stack',value=2),
                              step('assert_choices_consumed'),step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
                zones[0]['hand'].remove(removal);zones[0]['graveyard'].append(removal);totals[0]-=sum(cost_from_oracle(oracle[removal]))
                e=entities[target];e['alive']=False;zones[e['player']]['hand'].append(e['name'])
            else:resolve_equipment('source','host')
            steps.extend(stats())
            if mode=='move':activate('source','other');resolve_equipment('source','other')
            if mode=='cleanup':steps.extend([step('cleanup'),step('refresh')])
            if mode in ('source_bounce','host_bounce'):
                target='source' if mode=='source_bounce' else 'host';cast('removal',target)
                e=entities[target];e['alive']=False;zones[e['player']]['hand'].append(e['name']);attached.pop(target,None)
                for equipment,host in list(attached.items()):
                    if host==target:attached.pop(equipment)
            if mode in ('source_destroy','host_destroy'):destroy('source' if mode=='source_destroy' else 'host')
            if mode=='increase':
                cast('addition')
                if profile=='hand':zones[owner]['hand'].extend(zones[owner]['library'][:2]);del zones[owner]['library'][:2]
                if profile=='attachments':activate('addition','host');resolve_equipment('addition','host')
            if mode=='decrease':
                if profile=='hand':cast('addition')
                else:destroy('other' if profile=='creatures' else 'counted')
            if mode=='unblocked':
                steps.extend([step('begin_attackers'),step('attack',ref='host',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage')])
                # Keep combat life receipts separate from the common empty-combat checks.
                host=oracle[host_name];damage=int(host['power'])+amount('host')*p['power_scale']
                steps.extend([step('assert_life',player=1-owner,value=20-damage),step('assert_life',player=owner,value=20),step('end_combat')])
            if mode in ('tapped_source','tapped_host'):
                steps.append(step('require_card',ref='source' if mode=='tapped_source' else 'host',property='tapped',value=1))
        tail=checks()
        if mode=='unblocked':tail=[s for s in tail if s['op']!='assert_life']
        if mode=='exact_payment':tail += [step('assert_mana',player=owner,color=c,value=0) for c in range(6)]
        steps+=tail
        if mode in ('ready_snapshot','pending_snapshot'):
            cut=ready if mode=='ready_snapshot' else pending
            prefix=steps[:cut];replay=steps[cut:]
            steps=prefix+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('destroy',ref='other'),step('refresh'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'EquipmentWave7Recipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='equipment',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_components=['live color and mana-value changes','copy/control changes','attachment replacement effects','multiplayer'])))
    return result
