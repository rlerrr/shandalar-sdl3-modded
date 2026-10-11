"""Pinned, nontargeted temporary creature stats with frozen resolution cohorts."""
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

COLORS={'white':'W','blue':'U','black':'B','red':'R','green':'G'}
ARMY=('Savannah Lions','Merfolk of the Pearl Trident','Scathe Zombies','Grizzly Bears',
      'Hill Giant','Elvish Warrior','Phyrexian Hulk','Scaled Wurm','Wall of Stone')
WITNESSES={
    'Savannah Lions':('2','1',['W'],'Creature — Cat',''),
    'Merfolk of the Pearl Trident':('1','1',['U'],'Creature — Merfolk',''),
    'Scathe Zombies':('2','2',['B'],'Creature — Zombie',''),
    'Grizzly Bears':('2','2',['G'],'Creature — Bear',''),
    'Hill Giant':('3','3',['R'],'Creature — Giant',''),
    'Elvish Warrior':('2','3',['G'],'Creature — Elf Warrior',''),
    'Phyrexian Hulk':('5','4',[],'Artifact Creature — Phyrexian Golem',''),
    'Scaled Wurm':('7','6',['G'],'Creature — Wurm',''),
    'Wall of Stone':('0','8',['R'],'Creature — Wall',"Defender (This creature can't attack.)"),
}
NONCREATURES={'Darksteel Relic':('Artifact',[],"Indestructible (Effects that say \"destroy\" don't destroy this artifact.)"),
              'Maze of Ith':('Land',[], '{T}: Untap target attacking creature. Prevent all combat damage that would be dealt to and dealt by that creature this turn.')}
SPELLS={'Counterspell':('{U}{U}','Counter target spell.'),
        'Boomerang':('{U}{U}',"Return target permanent to its owner's hand."),
        'Ashcoat Bear':('{1}{G}','Flash (You may cast this spell any time you could cast an instant.)')}

def spec(d,r):
    from recipes import cost_from_oracle
    if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
        or r.get('type_line','').split(' — ')[0] not in ('Instant','Sorcery')
        or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
        raise ValueError('mass stats requires explicit variants and pinned normal instant/sorcery whole text')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'scope','state','predicate','value','power','toughness','draw','untap'}:
        raise ValueError('explicit controller, combat state, predicate, signed stats, draw and untap required')
    if p['scope'] not in ('own','opponents','both') or p['state'] not in ('all','attacking','blocking'):
        raise ValueError('unreviewed controller or combat restriction')
    if not ((p['predicate']=='all' and p['value']=='') or
            (p['predicate'] in ('color','not_color') and p['value'] in COLORS) or
            (p['predicate']=='not_subtype' and p['value']=='Elf')):
        raise ValueError('unreviewed creature predicate or witness')
    if (any(type(p[k]) is not int or not -4<=p[k]<=7 for k in ('power','toughness'))
        or not (p['power'] or p['toughness']) or type(p['draw']) is not int or p['draw'] not in (0,1)
        or type(p['untap']) is not bool):raise ValueError('bounded signed stats, zero/one draw and boolean untap required')
    if p['state']!='all' and (p['scope']!='both' or p['predicate']!='all' or r['type_line'].startswith('Sorcery')):
        raise ValueError('combat restriction requires reviewed global instant')
    noun=('' if p['predicate']=='all' else p['value']+' ' if p['predicate']=='color'
          else 'non'+p['value']+' ' if p['predicate']=='not_color' else 'non-'+p['value']+' ')
    stem=('' if p['state']=='all' else p['state']+' ')+noun+'creatures'+{'own':' you control','opponents':' your opponents control','both':''}[p['scope']]
    # Oracle uses both +0 and -0; zero has the same rules meaning.
    powers=('-0','+0') if p['power']==0 else (f"{p['power']:+d}",)
    toughnesses=('-0','+0') if p['toughness']==0 else (f"{p['toughness']:+d}",)
    alternatives={stem[0].upper()+stem[1:]+f' get {power}/{toughness} until end of turn.' for power in powers for toughness in toughnesses}
    if p['scope']=='both' and p['state']=='all' and p['predicate']=='all':alternatives |= {'All '+s[0].lower()+s[1:] for s in alternatives}
    if p['untap']:alternatives={s+' Untap those creatures.' for s in alternatives}
    if p['draw']:alternatives={s+sep+'Draw a card.' for s in alternatives for sep in (' ','\n')}
    if r.get('oracle_text') not in alternatives or r.get('keywords'):
        raise ValueError('additional keywords, triggers, conditions or costs require dedicated cases')
    return dict(p,family='mass_stats',mana=cost_from_oracle(r),instant=r['type_line'].startswith('Instant'))

def variants(p):
    names=['effect','payment','insufficient_mana','empty_battlefield','bare_zones','tapped','sick',
           'two_copies','cleanup','late_entry','removed_before','removed_after','bounced_before','bounced_after',
           'ready_snapshot','pending_snapshot','resolved_snapshot','countered','combat_blocked','combat_unblocked']
    if any(p['mana'][1:]):names+=['wrong_color']
    if p['instant']:names+=['opposing_turn','stacked_spells']
    return [v+'_'+str(owner) for owner in (0,1) for v in names]

def validate_witnesses(oracle,registry):
    for name,fields in WITNESSES.items():
        r=oracle.get(name,{})
        if name not in registry or r.get('layout')!='normal' or tuple(r.get(k) for k in ('power','toughness','colors','type_line','oracle_text'))!=fields:
            raise ValueError('mass stats army witness changed: '+name)
    for name,fields in NONCREATURES.items():
        r=oracle.get(name,{})
        if name not in registry or r.get('layout')!='normal' or tuple(r.get(k) for k in ('type_line','colors','oracle_text'))!=fields:
            raise ValueError('mass stats noncreature witness changed: '+name)
    for name,fields in SPELLS.items():
        r=oracle.get(name,{})
        if name not in registry or r.get('layout')!='normal' or tuple(r.get(k) for k in ('mana_cost','oracle_text'))!=fields:
            raise ValueError('mass stats spell witness changed: '+name)
    if (oracle.get('Ashcoat Bear',{}).get('power'),oracle.get('Ashcoat Bear',{}).get('toughness'))!=('2','2'):
        raise ValueError('late entrant characteristics changed')

def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step,observe
    result=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);empty=v=='empty_battlefield';bare=v=='bare_zones'
        hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
        exiles=[[] if bare else list(EXILE) for _ in (0,1)];libs=[list(LIBRARY) for _ in (0,1)]
        mana=[[10]*6,[10]*6];life=[20,20];entities={};setup=[];pre=[];actions=[]
        def add(nm,ref,player,zone='battlefield',state=None):
            item=card(nm,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='hand':hands[player].append(nm)
            elif zone=='battlefield':
                r=oracle[nm];entities[ref]=dict(name=nm,player=player,alive=True,tapped=bool(state and 'tapped' in state),
                     power=int(r.get('power','0')),toughness=int(r.get('toughness','0')),damage=0,attacking=False,blocking=False,creature='Creature' in r['type_line'])
        for player in (0,1):
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                setup += [card(nm,f'{zone}_{player}_{i}',player,zone) for i,nm in enumerate(names)]
            if not empty:
                for i,nm in enumerate(ARMY):
                    # Keep combat declaration participants ready; the rest of
                    # the board still probes tapped/sick creatures independently.
                    state=None if i==7 and p['state']!='all' else ['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None
                    add(nm,f'army_{player}_{i}',player,state=state)
                add('Darksteel Relic',f'artifact_{player}',player);add('Maze of Ith',f'land_{player}',player)
        add(name,'source',owner,'hand')
        if v in ('two_copies','stacked_spells'):add(name,'second',owner,'hand');mana[owner][0]=max(10,2*p['mana'][0])
        if v=='late_entry':add('Ashcoat Bear','late',owner,'hand')
        if v=='countered':add('Counterspell','response',0,'hand')
        if v in ('bounced_before','bounced_after'):add('Boomerang','response',0,'hand')
        actor=1-owner if p['state']=='blocking' or v=='opposing_turn' else owner
        pre.append(step('turn',player=actor,value=0))
        if v=='payment':mana[owner]=list(p['mana'])
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(p['mana']);c=next(c for c in range(1,6) if mana[owner][c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        for player in (0,1):pre += [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
        pre.append(step('refresh'))
        attacker=f'army_{actor}_7';blocker=f'army_{1-actor}_7'
        def combat_start(blocked):
            seq=[step('begin_attackers')]
            if not empty:
                seq += [step('attack',ref=attacker,value=1)];entities[attacker]['attacking']=True;entities[attacker]['tapped']=True
            seq += [step('finish_attackers'),step('begin_blockers')]
            if blocked and not empty:seq.append(step('block',ref=blocker,other=attacker,value=1));entities[blocker]['blocking']=True
            return seq+[step('advance_combat')]
        combat=v in ('combat_blocked','combat_unblocked') or p['state']!='all'
        blocked=v!='combat_unblocked'
        if combat and (p['instant'] or p['state']!='all'):pre+=combat_start(blocked)
        def matches(e):
            if not e['alive'] or not e['creature']:return False
            if p['scope']=='own' and e['player']!=owner or p['scope']=='opponents' and e['player']==owner:return False
            if p['state']!='all' and not e[p['state']]:return False
            r=oracle[e['name']]
            if p['predicate']=='color':return COLORS[p['value']] in r['colors']
            if p['predicate']=='not_color':return COLORS[p['value']] not in r['colors']
            if p['predicate']=='not_subtype':return p['value'] not in r['type_line'].split(' — ')[-1].split()
            return True
        def die(ref):
            e=entities[ref];e['alive']=False;graves[e['player']].append(e['name'])
        def apply_effect():
            for ref,e in entities.items():
                if matches(e):
                    e['power']+=p['power'];e['toughness']+=p['toughness']
                    if p['untap']:e['tapped']=False
                    if e['toughness']<=0:die(ref)
            hands[owner].extend(libs[owner][:p['draw']]);del libs[owner][:p['draw']]
        def zones(pending=False):
            seq=[]
            for player in (0,1):
                for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                    if pending and zone=='hand':continue
                    seq.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                    seq += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
                if not pending:seq += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
            return seq
        def stats():
            seq=[]
            for ref,e in entities.items():
                seq.append(observe(ref,'in_play',int(e['alive'])))
                if e['alive']:
                    seq.append(observe(ref,'tapped',int(e['tapped'])))
                    if e['creature']:seq += [observe(ref,k,e[k]) for k in ('power','toughness','damage')]
            return seq
        def pay(cost,player=owner):mana[player]=[a-b for a,b in zip(mana[player],cost)]
        def cast(ref='source'):
            pay(p['mana']);hands[owner].remove(name)
            return [step('cast',ref=ref,value=1),step('assert_choices_consumed')]
        def resolve():
            apply_effect();graves[owner].append(name)
            return [step('resolve'),step('assert_choices_consumed')]
        def bounce():
            e=entities[f'army_{owner}_7'];e['alive']=False;hands[owner].append(e['name']);hands[0].remove('Boomerang');graves[0].append('Boomerang');pay([0,0,2,0,0,0],0)
            return [step('choose_target',player=0,ref=f'army_{owner}_7'),step('cast',ref='response',value=1),step('resolve'),step('assert_choices_consumed')]
        ready=0;pending_cut=0;illegal=v in ('insufficient_mana','wrong_color')
        if illegal:actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')]
        elif v=='countered':
            pay(p['mana']);pay([0,0,2,0,0,0],0);hands[owner].remove(name);hands[0].remove('Counterspell');graves[owner].append(name);graves[0].append('Counterspell')
            actions += [step('choose_priority',ref='response'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
        else:
            actions+=cast()+[step('assert_stack',value=1)]+zones(pending=True)+stats();pending_cut=len(actions)
            if v=='removed_before':actions.append(step('destroy',ref=f'army_{owner}_7'));die(f'army_{owner}_7')
            if v=='bounced_before':actions+=bounce()
            if v=='stacked_spells':actions+=cast('second')+[step('assert_stack',value=2)];apply_effect();graves[owner].append(name)
            actions+=resolve()+stats()+zones()
            if v=='two_copies':actions+=cast('second')+[step('assert_stack',value=1)]+resolve()+stats()+zones()
            if v=='removed_after':actions.append(step('destroy',ref=f'army_{owner}_7'));die(f'army_{owner}_7')
            if v=='bounced_after':actions+=bounce()
            if v=='late_entry':
                hands[owner].remove('Ashcoat Bear');pay([1,0,0,1,0,0]);r=oracle['Ashcoat Bear'];entities['late']=dict(name='Ashcoat Bear',player=owner,alive=True,tapped=False,power=2,toughness=2,damage=0,attacking=False,blocking=False,creature=True)
                actions += [step('cast',ref='late',value=1),step('resolve'),step('assert_choices_consumed')]
            if combat and not p['instant']:actions+=combat_start(blocked)
            if v in ('combat_blocked','combat_unblocked'):
                a=entities[attacker];b=entities[blocker]
                ap=max(0,a['power']);bp=max(0,b['power'])
                if blocked:
                    a['damage']+=bp;b['damage']+=ap
                    if a['damage']>=a['toughness']:die(attacker)
                    if b['damage']>=b['toughness']:die(blocker)
                else:life[1-actor]-=ap
                actions += [step('combat_damage'),step('end_combat')]
                for e in entities.values():e['attacking']=False;e['blocking']=False
            if v=='cleanup':
                actions += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]+[step('cleanup')]
                for e in entities.values():
                    if e['alive'] and e['creature']:r=oracle[e['name']];e['power']=int(r['power']);e['toughness']=int(r['toughness']);e['damage']=0
        checks=zones()+stats()+[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
            if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot'):
            cut=ready if v=='ready_snapshot' else pending_cut
            steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'MassStatsRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
             _recipe=dict(family='mass_stats',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['Arcane subtype interactions','multiplayer'])))
    return result
