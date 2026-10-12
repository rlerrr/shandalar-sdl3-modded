"""Fixed token spells and entry triggers; expectations come from creation text."""
import copy
from collections import Counter
from creature_recipes import classify
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

COLORS={'white':'W','blue':'U','black':'B','red':'R','green':'G'}
SUBTYPES={'Angel','Bear','Bird','Centaur','Citizen','Dragon','Drake','Eldrazi','Elf','Goblin',
 'Horror','Human','Insect','Knight','Ally','Minotaur','Myr','Satyr','Servo','Sliver','Soldier','Spirit','Thopter','Warrior','Wurm'}
KEYWORDS={'Flying','Trample','Haste','Vigilance','Deathtouch'}
WORDS={1:'a',2:'two',3:'three',4:'four'}
WITNESSES={
 'Grizzly Bears':('{1}{G}','','2','2'), 'Giant Spider':('{3}{G}','Reach (This creature can block creatures with flying.)','2','4'),
 'Suntail Hawk':('{W}','Flying','1','1'),
 'Unsummon':('{U}',"Return target creature to its owner's hand."),
 'Cloudshift':('{W}','Exile target creature you control, then return that card to the battlefield under your control.'),
 'Swords to Plowshares':('{W}','Exile target creature. Its controller gains life equal to its power.'),
 'Wrath of God':('{2}{W}{W}',"Destroy all creatures. They can't be regenerated."),
 'Counterspell':('{U}{U}','Counter target spell.'),
}

def spec(d,r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
        raise ValueError('token creation requires a whole pinned normal card and explicit variants')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'keywords','token'}:raise ValueError('explicit source keywords and one fixed token definition required')
    t=p['token']
    if not isinstance(t,dict) or set(t)!={'name','count','power','toughness','colors','subtypes','artifact','keywords'}:
        raise ValueError('complete explicit token definition required')
    for k,lo,hi in (('count',1,4),('power',1,8),('toughness',1,8)):
        if type(t[k]) is not int or not lo<=t[k]<=hi:raise ValueError('bounded positive token characteristics required')
    if type(t['artifact']) is not bool:raise ValueError('explicit boolean artifact characteristic required')
    for k,allowed in (('colors',set(COLORS.values())),('subtypes',SUBTYPES),('keywords',KEYWORDS)):
        if not isinstance(t[k],list) or any(not isinstance(x,str) or x not in allowed for x in t[k]) or len(set(t[k]))!=len(t[k]):
            raise ValueError('reviewed unique token colors, subtypes and keywords required')
    if not t['subtypes'] or len(t['subtypes'])>2 or len(t['colors'])>2 or len(t['keywords'])>2 or t['name']!=' '.join(t['subtypes']):
        raise ValueError('ordinary unnamed token with reviewed bounded characteristics required')
    color=' and '.join(next(n for n,c in COLORS.items() if c==v) for v in t['colors']) or 'colorless'
    clause=f"Create {WORDS[t['count']]} {t['power']}/{t['toughness']} {color} {t['name']} "+('artifact ' if t['artifact'] else '')+'creature token'+('s' if t['count']>1 else '')
    if t['keywords']:clause+=' with '+' and '.join(k.lower() for k in t['keywords'])
    clause+='.'
    creature='Creature' in r['type_line'];lines=r['oracle_text'].splitlines();trigger=None
    if creature:
        expected={f'When {ref} enters{suffix}, '+clause[0].lower()+clause[1:] for ref in ('this creature',r['name']) for suffix in ('',' the battlefield')}
        triggers=[l for l in lines if l in expected]
        if len(triggers)!=1:raise ValueError('single fixed entry creation clause required')
        trigger=triggers[0];base=classify(dict(r,oracle_text='\n'.join(l for l in lines if l!=trigger)))
        if p['keywords']!=base['keywords']:raise ValueError('explicit unconditional source keywords required')
    elif any(k in r['type_line'] for k in ('Instant','Sorcery')):
        if lines!=[clause] or p['keywords'] or r.get('keywords'):raise ValueError('ordinary fixed token spell without additional clauses required')
        base=dict(mana=cost_from_oracle(r),power=0,toughness=0)
    else:raise ValueError('entry creature or ordinary instant/sorcery required')
    return dict(p,family='token_creation',creature=creature,mana=base['mana'],power=base['power'],toughness=base['toughness'])

def variants(p):
    common=['create','payment','insufficient_mana','wrong_color','two_casts','cleanup','bare_zones',
      'ready_snapshot','pending_snapshot','resolved_snapshot','countered','fresh_attack','ready_attack',
      'block_ground','block_flying','destroy_token','bounce_token','exile_token','wrath','other_creature']
    if not any(p['mana'][1:]):common.remove('wrong_color')
    if p['creature']:common+=['source_destroyed','source_bounced','bounce_recast','blink','blink_snapshot','preloaded_source']
    if 'Flying' in p['token']['keywords']:common+=['evasion','reach_blocker']
    if 'Trample' in p['token']['keywords']:common+=['trample']
    return [v+'_'+str(owner) for owner in (0,1) for v in common]

def validate_witnesses(o,rows,p):
    for name,fields in WITNESSES.items():
        r=o.get(name,{})
        if name not in rows or tuple(r.get(k) for k in ('mana_cost','oracle_text','power','toughness')[:len(fields)])!=fields:
            raise ValueError('token creation witness changed: '+name)
    token=rows.get(p['token']['name'])
    if not token or not o.get(p['token']['name'],{}).get('type_line','').startswith('Token '):
        raise ValueError('registered token identity required; characteristics come from the maker Oracle text')

def expand(name,csvid,d,p,selected,rows,o):
    from recipes import card,step,observe,cost_from_oracle
    result=[];t=p['token'];token_id=rows[t['name']]['csvid']
    for requested in selected:
        v,owner=requested.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
        hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
        libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
        # Battlefield fixture loading uses put_into_play, so its entry trigger
        # runs normally even though no spell is cast and no mana is paid.
        mana=[[10]*6,[10]*6];life=[20,20];setup=[];entities={};count=t['count'] if v=='preloaded_source' else 0;token_tapped=False;token_damage=0
        pre=[step('turn',player=owner,value=0)];actions=[]
        def add(nm,ref,player,zone='hand'):
            setup.append(card(nm,ref,player,zone));r=o[nm];creature='Creature' in r['type_line']
            entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0,tapped=False)
            if zone=='hand':hands[player].append(nm)
        def pay(player,cost):mana[player]=[a-b for a,b in zip(mana[player],cost)]
        def leave(ref,zone):
            e=entities[ref];e['alive']=False;{'hand':hands,'graveyard':graves}[zone][e['player']].append(e['name'])
        def source_cast(ref='source'):
            nonlocal count
            hands[owner].remove(name);pay(owner,p['mana']);e=entities[ref]
            if p['creature']:e['alive']=True
            else:graves[owner].append(name)
            count+=t['count']
            return [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
        def spell(nm,ref,target=None):
            add(nm,ref,0);hands[0].remove(nm);graves[0].append(nm);pay(0,cost_from_oracle(o[nm]))
            return ([step('choose_target',player=0,ref=target)] if target else [])+[step('cast',ref=ref,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
        add(name,'source',owner,'battlefield' if v=='preloaded_source' else 'hand');add('Grizzly Bears','unselected',owner,'battlefield')
        if v=='two_casts':add(name,'second',owner)
        if v in ('payment','insufficient_mana','wrong_color'):
            pool=list(p['mana']) if v!='insufficient_mana' else [0]*6
            if v=='wrong_color':
                c=next(c for c in range(1,6) if pool[c]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
            mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
        if v in ('insufficient_mana','wrong_color'):
            actions += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
        elif v=='countered':
            add('Counterspell','counter',0);hands[owner].remove(name);hands[0].remove('Counterspell');graves[owner].append(name);graves[0].append('Counterspell');pay(owner,p['mana']);pay(0,cost_from_oracle(o['Counterspell']))
            actions += [step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
        elif v!='preloaded_source':actions += source_cast()
        if v=='two_casts':actions+=source_cast('second')
        if v=='other_creature':
            add('Grizzly Bears','other',owner);hands[owner].remove('Grizzly Bears');pay(owner,cost_from_oracle(o['Grizzly Bears']));entities['other']['alive']=True
            actions += [step('cast',ref='other',value=1),step('resolve'),step('assert_choices_consumed')]
        if v=='source_destroyed':actions += [step('destroy',ref='source')];leave('source','graveyard')
        if v in ('source_bounced','bounce_recast'):
            actions+=spell('Unsummon','bounce','source');leave('source','hand');actions += [step('bind_hand',player=owner,value=csvid,ref='source')]
            if v=='bounce_recast':actions+=source_cast()
        if v in ('blink','blink_snapshot'):
            # Cloudshift must target a creature its caster controls.
            add('Cloudshift','blink',owner);hands[owner].remove('Cloudshift');graves[owner].append('Cloudshift');pay(owner,cost_from_oracle(o['Cloudshift']));count+=t['count']
            actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='blink',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('bind_permanent',player=owner,value=csvid,ref='source')]
        if v=='cleanup':actions += [step('cleanup'),step('refresh')]
        if count and v in ('fresh_attack','ready_attack','block_ground','block_flying','evasion','reach_blocker','trample','destroy_token','bounce_token','exile_token'):
            actions += [step('bind_permanent_copy',player=owner,value=token_id,index=0,ref='token')]
        if v=='destroy_token':actions += [step('destroy',ref='token'),step('refresh')];count-=1
        if v=='bounce_token':actions+=spell('Unsummon','bounce','token');count-=1
        if v=='exile_token':actions+=spell('Swords to Plowshares','exile','token');life[owner]+=t['power'];count-=1
        if v=='wrath':
            actions+=spell('Wrath of God','wrath');count=0
            for ref,e in entities.items():
                if e['alive']:leave(ref,'graveyard')
        combat=v in ('fresh_attack','ready_attack','block_ground','block_flying','evasion','reach_blocker','trample')
        if combat:
            defending=v in ('block_ground','block_flying');keys=set(t['keywords'])
            if not defending and v!='fresh_attack':actions += [step('advance_turn'),step('advance_turn')]
            witness='Suntail Hawk' if v=='block_flying' else 'Giant Spider' if v=='reach_blocker' else 'Grizzly Bears' if defending or v in ('evasion','trample') else None
            if witness:add(witness,'witness',1-owner,'battlefield')
            attacker='witness' if defending else 'token';blocker='token' if defending else 'witness';can_attack=defending or v!='fresh_attack' or 'Haste' in keys
            actions += [step('turn',player=1-owner if defending else owner,value=0),step('begin_attackers'),step('attack',ref=attacker,value=int(can_attack)),step('finish_attackers'),step('begin_blockers')]
            blocks=bool(witness and can_attack and (not defending and ('Flying' not in keys or v=='reach_blocker') or defending and (v!='block_flying' or 'Flying' in keys)))
            if witness and can_attack:actions += [step('block',ref=blocker,other=attacker,value=int(blocks))]
            actions += [step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
            if defending:entities['witness']['tapped']=True
            elif can_attack:token_tapped='Vigilance' not in keys
            if blocks:
                w=entities['witness'];deathtouch='Deathtouch' in keys
                lethal=1 if deathtouch else w['toughness'];dead_w=t['power']>=lethal;dead_token=w['power']>=t['toughness']
                if not defending and 'Trample' in keys:life[1-owner]-=max(0,t['power']-lethal)
                if dead_w:leave('witness','graveyard')
                else:w['damage']=t['power']
                if dead_token:count-=1;token_tapped=False
                else:token_damage=w['power']
            elif can_attack:life[owner if defending else 1-owner]-=entities['witness']['power'] if defending else t['power']
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            checks.append(observe(ref,'in_play',int(e['alive'])))
            if e['alive']:
                checks.append(observe(ref,'tapped',int(e['tapped'])))
                if e['creature']:checks += [observe(ref,k,e[k]) for k in ('power','toughness','damage')]
        if count:
            properties=dict(token=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in t['colors']),artifact=int(t['artifact']),power=t['power'],toughness=t['toughness'])
            properties.update({'subtype_'+n:1 for n in t['subtypes']})
            for prop,value in properties.items():checks.append(step('assert_matching_permanents',player=owner,index=token_id,property=prop,value=value))
            for n in range(count):
                ref='expected_token_'+str(n);checks += [step('bind_permanent_copy',player=owner,value=token_id,index=n,ref=ref),observe(ref,'tapped',int(n==0 and token_tapped)),observe(ref,'damage',token_damage if n==0 else 0)]
        universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}|{t['name']}
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values())+(count if player==owner else 0)),step('assert_zone_multiplicity',player=player,zone='battlefield',index=token_id,value=count if player==owner else 0)]
            checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=n,value=rows[nm]['csvid']) for n,nm in enumerate(names)]
                else:
                    counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','blink_snapshot'):
            cut=0 if v=='ready_snapshot' else 1 if v=='pending_snapshot' else len(actions)-6
            tail=actions[cut:]+checks
            steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'TokenCreationRecipe_{csvid}_{requested}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
          _recipe=dict(family='token_creation',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=['source passive abilities','separate entry-trigger priority window','control changes','copy effects','token doublers','multiple simultaneous damage assignments'])))
    return result
