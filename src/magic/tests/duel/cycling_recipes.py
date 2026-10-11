"""Ordinary cycling through the engine's hand-ability proxy, not direct events."""
import re
from collections import Counter
from grant_recipes import HAND,GRAVE,EXILE,LIBRARY

WITNESSES={
    'Fluctuator':('{2}','Cycling abilities you activate cost {2} less to activate.'),
    'Suppression Field':('{1}{W}',"Activated abilities cost {2} more to activate unless they're mana abilities."),
    'Reclaim':('{G}','Put target card from your graveyard on top of your library.'),
    "Tormod's Crypt":('{0}',"{T}, Sacrifice this artifact: Exile target player's graveyard."),
}

def spec(declaration,record):
    from recipes import cost_from_oracle
    if not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or declaration.get('oracle_text')!=record.get('oracle_text') or 'variants' not in declaration:
        raise ValueError('cycling requires pinned normal single-face text and reviewed variants')
    p=declaration.get('params')
    if not isinstance(p,dict) or set(p)!={'cycling_cost'} or not isinstance(p['cycling_cost'],str) or not re.fullmatch(r'(?:\{(?:[1-7]|[BUGRW])\})+',p['cycling_cost']):
        raise ValueError('explicit bounded ordinary cycling cost required')
    cost=p['cycling_cost'];pool=cost_from_oracle(dict(mana_cost=cost))
    if not 1<=sum(pool)<=8:raise ValueError('cycling cost outside reviewed range')
    clause=f'Cycling {cost}';allowed={clause,clause+f' ({cost}, Discard this card: Draw a card.)'}
    lines=record['oracle_text'].splitlines();matching=[l for l in lines if l in allowed]
    other=[l for l in lines if l not in allowed]
    # Cast text is outside this component. Cycling/discard triggers and other
    # graveyard/hand abilities need dedicated scripts rather than silent gaps.
    if len(matching)!=1 or 'Cycling' not in record.get('keywords',[]) or re.search(r'cycl|madness|unearth|flashback', '\n'.join(other),re.I):
        raise ValueError('extra cycling or alternate graveyard mechanic requires dedicated cases')
    if 'Instant' not in record['type_line'] and 'Sorcery' not in record['type_line'] and re.search(r'discard|graveyard|from your hand|return this card','\n'.join(other),re.I):
        raise ValueError('extra hand/discard/graveyard rule requires dedicated cases')
    return dict(family='cycling',cycling_cost=cost,activation_mana=pool,untested_rules=other,
                untested_keywords=[k for k in record.get('keywords',[]) if k!='Cycling'])

def variants(p):
    modes=['cycle','payment','pending_snapshot','resolved_snapshot','two_copies','bare_zones','duplicate_draw',
           'one_card_library','enemy_turn','combat','cleanup_pending','cancel','insufficient_mana',
           'reducer','opposing_reducer','two_reducers','tax','opposing_tax','two_taxes','reducer_tax',
           'reclaim_pending','crypt_pending']
    if any(p['activation_mana'][1:]):modes+=['wrong_color','reducer_missing_color']
    if p['activation_mana'][0]==0:modes+=['reducer_no_mana']
    return [m+'_'+str(owner) for owner in (0,1) for m in modes]

def validate_witnesses(oracle,registry):
    for name,fields in WITNESSES.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=fields:
            raise ValueError('cycling witness changed: '+name)
    r=oracle.get('Savannah Lions',{})
    if 'Savannah Lions' not in registry or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=('2','1',''):
        raise ValueError('cycling combat witness changed')

def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,cost_from_oracle
    cases=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner)
        custom=v in ('bare_zones','duplicate_draw','one_card_library')
        fixture='empty_main' if custom else 'stocked_zones'
        hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];exile=[list(EXILE),list(EXILE)];libs=[list(LIBRARY),list(LIBRARY)]
        setup=[];pre=[];run=[];mana=[[10]*6,[10]*6];battle=[0,0]
        if v=='bare_zones':hands=[[],[]];graves=[[],[]];exile=[[],[]];libs=[['Forest'],['Forest']]
        if v=='duplicate_draw':libs[owner]=['Forest','Forest','Plains','Grizzly Bears']
        if v=='one_card_library':libs[owner]=['Forest']
        if custom:
            for player in (0,1):
                for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exile[player]),('library',libs[player])):
                    setup += [card(nm,f'p{player}_{zone}_{i}',player,zone) for i,nm in enumerate(names)]
        zone='hand'
        setup.append(card(name,'source',owner,zone));{'hand':hands,'graveyard':graves,'exile':exile}[zone][owner].append(name)
        pre.append(step('turn',player=1-owner if v=='enemy_turn' else owner,value=0))
        cost=list(p['activation_mana']);reducers=0;taxes=0
        for nm,players in (
            ('Fluctuator',[owner] if v in ('reducer','reducer_tax','reducer_missing_color','reducer_no_mana') else [1-owner] if v=='opposing_reducer' else [owner,owner] if v=='two_reducers' else []),
            ('Suppression Field',[owner] if v in ('tax','reducer_tax') else [1-owner] if v=='opposing_tax' else [owner,1-owner] if v=='two_taxes' else [])):
            for i,player in enumerate(players):
                setup.append(card(nm,f'{nm}_{i}',player));battle[player]+=1
                if nm=='Fluctuator':reducers+=player==owner
                else:taxes+=1
        cost[0]=max(0,cost[0]+2*taxes-2*reducers)
        if v in ('payment','reducer','opposing_reducer','two_reducers','tax','opposing_tax','two_taxes','reducer_tax'):
            mana[owner]=list(cost)
        legal=v not in ('insufficient_mana','wrong_color','reducer_missing_color','reducer_no_mana')
        if v in ('insufficient_mana','reducer_no_mana'):mana[owner]=[0]*6
        if v in ('wrong_color','reducer_missing_color'):
            mana[owner]=list(cost);color=next(i for i in range(1,6) if cost[i]);mana[owner][color]=0;mana[owner][1 if color!=1 else 2]+=cost[color]
        pre += [step('set_mana',player=owner,color=i,value=value) for i,value in enumerate(mana[owner])]
        if v=='combat':
            setup.append(card('Savannah Lions','attacker',owner));battle[owner]+=1
            pre += [step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers')]
        if v=='two_copies':setup.append(card(name,'second',owner,'hand'));hands[owner].append(name)
        # Human-controlled Crypt can target either player without AI strategic refusal.
        if v=='crypt_pending':setup.append(card("Tormod's Crypt",'crypt',0));battle[0]+=1
        if v=='reclaim_pending':setup.append(card('Reclaim','reclaim',owner,'hand'))
        def library_checks():
            return [step('assert_zone',player=owner,zone='library',value=len(libs[owner]))]+[step('assert_zone_card',player=owner,zone='library',index=i,value=registry[nm]['csvid']) for i,nm in enumerate(libs[owner])]
        def activate(ref):
            hands[owner].remove(name);graves[owner].append(name)
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)]
            return [step('require_can_activate_hand',ref=ref,value=1),step('choose_dialog',player=owner,value=1),step('activate_hand',ref=ref,value=1),step('assert_stack',value=1),step('assert_choices_consumed'),
                    step('assert_hand_graveyard_total',player=owner,index=csvid,value=hands[owner].count(name)+graves[owner].count(name)),step('assert_zone',player=owner,zone='graveyard',value=len(graves[owner])),
                    step('assert_mana_total',player=owner,value=sum(mana[owner]))]+library_checks()
        def resolve():
            drawn=libs[owner].pop(0);hands[owner].append(drawn)
            return [step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_zone_contains',player=owner,zone='hand',value=registry[drawn]['csvid'])]+library_checks()
        cut=0
        if not legal:run += [step('can_activate_hand',ref='source',value=0),step('assert_stack',value=0)]
        elif v=='cancel':run += [step('require_can_activate_hand',ref='source',value=1),step('choose_dialog',player=owner,value=0),step('activate_hand',ref='source',value=0),step('assert_choices_consumed'),observe('source','in_hand',1),step('assert_stack',value=0)]
        else:
            run+=activate('source');cut=len(run)
            if v=='cleanup_pending':run += [step('cleanup'),step('assert_stack',value=1)]+library_checks()
            if v=='reclaim_pending':
                run += [step('choose_zone',player=owner,value=len(graves[owner])-1),step('cast',ref='reclaim',value=1),step('assert_stack',value=2)]
                graves[owner].remove(name);graves[owner].append('Reclaim');libs[owner].insert(0,name);mana[owner][3]-=1
            if v=='crypt_pending':
                run += [step('choose_player',player=0,value=owner),step('activate',ref='crypt',value=1),step('assert_stack',value=2),step('assert_zone',player=0,zone='graveyard',value=len(graves[0])+1)]
                graves[0].append("Tormod's Crypt");exile[owner]+=graves[owner];graves[owner]=[];battle[0]-=1
            run+=resolve()
            if v=='two_copies':run+=activate('second')+resolve()
            if v=='resolved_snapshot':cut=len(run)
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=20),step('assert_mana_total',player=player,value=sum(mana[player])),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exile[player]),('library',libs[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
        actions=pre+run+checks
        if v in ('pending_snapshot','resolved_snapshot'):
            actions=pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        cases.append(dict(name=f'CyclingRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=actions,
                          _recipe=dict(family='cycling',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_rules=p['untested_rules'],untested_keywords=p['untested_keywords'],untested_components=['casting and spell effects','battlefield abilities','multiplayer'])))
    return cases
