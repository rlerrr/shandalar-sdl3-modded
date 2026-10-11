"""Pinned infect/wither components through ordinary combat and persistent counters."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

MECHANICS = {
    'Infect': '(This creature deals damage to creatures in the form of -1/-1 counters and to players in the form of poison counters.)',
    'Wither': '(This deals damage to creatures in the form of -1/-1 counters.)',
}
WITNESSES = {
    'Wall of Denial': ('0','8',['Defender','Flying','Shroud'],'Defender, flying\nShroud (This creature can\'t be the target of spells or abilities.)'),
    'Ornithopter': ('0','2',['Flying'],'Flying'),
    'Phyrexian Walker': ('0','3',[],''),
    'Hill Giant': ('3','3',[],''),
}
SPELLS = {'Giant Growth': ('{G}','Target creature gets +3/+3 until end of turn.'),
          'Unsummon': ('{U}',"Return target creature to its owner's hand.")}

def spec(d,r):
    from recipes import cost_from_oracle
    if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line','') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
        raise ValueError('counter damage requires a pinned normal creature and explicit variants')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'mechanic','keywords'} or p['mechanic'] not in MECHANICS:
        raise ValueError('explicit infect/wither mechanic and passive keywords required')
    keys=[];mechanics=[];abilities=[];canonical={k.lower():k for k in KEYWORDS}
    for line in r['oracle_text'].splitlines():
        if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):
            abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS|set(MECHANICS.values()):raise ValueError('unreviewed counter damage reminder')
            text=text.replace(reminder,'')
        for token in re.split(r',|\n',text.strip()):
            token=token.strip()
            if token in MECHANICS:mechanics.append(token)
            elif token.lower() in canonical:keys.append(canonical[token.lower()])
            else:raise ValueError('unreviewed counter damage passive or triggered rule')
    if (mechanics!=[p['mechanic']] or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys))
            or set(keys)|set(mechanics)!=set(r.get('keywords',[]))
            or set(keys)&{'Menace','Deathtouch','Trample','Double strike'}):
        raise ValueError('counter damage parameters disagree with whole passive scope')
    for k in ('power','toughness'):
        if not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=5:raise ValueError('bounded numeric counter damage stats required')
    if int(r['toughness'])==0:raise ValueError('counter damage source must survive entry')
    return dict(p,family='counter_damage_creature',power=int(r['power']),toughness=int(r['toughness']),
                mana=cost_from_oracle(r),remaining_abilities=abilities)

def variants(p):
    modes=['cast','payment','insufficient_mana','fresh_attack','unblocked','blocked','lethal_blocker',
           'defend_ground','defend_flying','other_attack','tapped','sick','pump','cleanup','destroy_after',
           'bounce_after','ready_snapshot','combat_snapshot','resolved_snapshot','bare_zones']
    if any(p['mana'][1:]):modes.append('wrong_color')
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]+['payment']

def validate_witnesses(o,registry):
    for nm,values in WITNESSES.items():
        r=o.get(nm,{})
        if nm not in registry or r.get('layout')!='normal' or (r.get('power'),r.get('toughness'),sorted(r.get('keywords',[])),r.get('oracle_text'))!=(values[0],values[1],sorted(values[2]),values[3]):
            raise ValueError('counter damage witness changed: '+nm)
    for nm,values in SPELLS.items():
        r=o.get(nm,{})
        if nm not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=values:raise ValueError('counter damage spell changed: '+nm)

def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step,observe
    result=[];keys=set(p['keywords']);infect=p['mechanic']=='Infect'
    for variant in selected:
        v,owner=variant.rsplit('_',1) if variant!='payment' else ('payment','0');owner=int(owner)
        bare=v=='bare_zones';full=not bare
        hands=[list(HAND) if full else [] for _ in (0,1)];graves=[list(GRAVE) if full else [] for _ in (0,1)]
        exiles=[list(EXILE) if full else [] for _ in (0,1)];libs=[list(LIBRARY) if full else [] for _ in (0,1)]
        mana=[[10]*6,[10]*6];life=[20,20];poison=[0,0];setup=[];entities={};pre=[];actions=[]
        def add(nm,ref,player,zone='battlefield',state=None):
            item=card(nm,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='hand':hands[player].append(nm)
            r=oracle[nm];entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',hand=zone=='hand',power=int(r['power']),toughness=int(r['toughness']),minus1_counters=0,damage=0,tapped=bool(state and 'tapped' in state),attacking=False)
        casting=v in ('cast','payment','insufficient_mana','wrong_color','fresh_attack')
        add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
        add('Hill Giant','idle',owner)
        defending=v.startswith('defend_');witness='Phyrexian Walker' if v=='defend_ground' else 'Ornithopter' if v in ('defend_flying','lethal_blocker') else 'Wall of Denial'
        add(witness,'witness',1-owner)
        actor=1-owner if defending else owner;pre.append(step('turn',player=actor,value=0))
        # AI selects any affordable hybrid vector; exact pools make the
        # payment deterministic without asserting an AI preference.
        if owner==1 and v in ('cast','fresh_attack'):mana[owner]=list(p['mana'])
        if v=='payment':mana[owner]=list(p['mana'])
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(p['mana']);c=next(c for c in range(1,6) if mana[owner][c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        pre += [step('set_mana',player=player,color=c,value=n) for player in (0,1) for c,n in enumerate(mana[player])]
        def stats():
            out=[]
            for ref,e in entities.items():
                out += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
                if e['alive']:out += [observe(ref,k,int(e[k])) for k in ('power','toughness','minus1_counters','damage','tapped','attacking')]+[observe(ref,'plus1_counters',0)]
            return out
        def die(ref):
            e=entities[ref];e['alive']=False;e['hand']=False;graves[e['player']].append(e['name'])
        if casting:
            legal=v not in ('insufficient_mana','wrong_color');actions += [step('cast',ref='source',value=int(legal)),step('assert_choices_consumed')]
            if legal:
                actions += [step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
                mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])];hands[owner].remove(name);entities['source']['alive']=True;entities['source']['hand']=False
        if v=='pump':
            setup.append(card('Giant Growth','pump',owner,'hand'));mana[owner][3]-=1;graves[owner].append('Giant Growth')
            actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='pump',value=1),step('resolve'),step('assert_choices_consumed')]
            entities['source']['power']+=3;entities['source']['toughness']+=3
        combat=not casting or v=='fresh_attack';cut=0
        if combat:
            actions += [step('begin_attackers')];attacking=[]
            if defending:
                actions += [step('attack',ref='witness',value=1)];attacking=['witness']
            elif v=='other_attack':
                actions += [step('attack',ref='idle',value=1)];attacking=['idle']
            else:
                legal='Defender' not in keys and v!='tapped' and (v!='sick' or 'Haste' in keys) and (v!='fresh_attack' or 'Haste' in keys)
                actions += [step('attack',ref='source',value=int(legal))]
                if legal:attacking=['source']
            for ref in attacking:entities[ref]['attacking']=True;entities[ref]['tapped']='Vigilance' not in keys if ref=='source' else True
            actions += [step('finish_attackers')]+stats();cut=len(actions)
            actions += [step('begin_blockers')];blocked=False
            if defending:
                blocked=v=='defend_ground' or bool(keys&{'Flying','Reach'})
                actions += [step('block',ref='source',other='witness',value=int(blocked))]
            elif v in ('blocked','lethal_blocker','cleanup','combat_snapshot','resolved_snapshot','destroy_after','bounce_after') and attacking:
                blocked=True;actions += [step('block',ref='witness',other='source',value=1)]
            actions += [step('advance_combat')]
            if blocked:
                amount=entities['source']['power'];victim=entities['witness'];victim['minus1_counters']+=amount;victim['power']-=amount;victim['toughness']-=amount
                if victim['toughness']<=0:die('witness')
                if 'Lifelink' in keys:life[owner]+=amount
            elif attacking:
                amount=entities[attacking[0]]['power']
                if attacking==['source'] and infect:poison[1-owner]+=amount
                else:life[1-actor]-=amount
                if attacking==['source'] and 'Lifelink' in keys:life[owner]+=amount
            actions += [step('combat_damage'),step('end_combat')]
            for e in entities.values():e['attacking']=False
        if v=='destroy_after':actions += [step('destroy',ref='source')];die('source')
        if v=='bounce_after':
            setup.append(card('Unsummon','bounce',0,'hand'));mana[0][2]-=1;graves[0].append('Unsummon');hands[owner].append(name);entities['source']['alive']=False;entities['source']['hand']=True
            actions += [step('choose_target',player=0,ref='source'),step('cast',ref='bounce',value=1),step('resolve'),step('assert_choices_consumed'),step('bind_hand',player=owner,value=csvid,ref='source')]
        if v=='cleanup':actions+=stats()+[step('cleanup')]
        checks=stats()+[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_poison',player=player,value=poison[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                checks += [step('assert_zone',player=player,zone=zone,value=len(names))]
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
            if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
        seq=pre+actions+checks
        if v in ('ready_snapshot','combat_snapshot'):
            split=0 if v=='ready_snapshot' else cut
            seq=pre+actions[:split]+[step('capture')]+actions[split:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[split:]+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'CounterDamageCreatureRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=seq,_recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['remaining_abilities']+['noncombat damage','counter cancellation','multiple blockers','Flash timing'])))
    return result
