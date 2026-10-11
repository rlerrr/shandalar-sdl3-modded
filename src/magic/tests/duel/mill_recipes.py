"""Fixed library-to-graveyard effects; expectations come from pinned Oracle text."""
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

WORDS = {1:'a card',2:'two cards',3:'three cards',4:'four cards',5:'five cards',6:'six cards',7:'seven cards',8:'eight cards',9:'nine cards',10:'ten cards'}
WITNESSES = {'Counterspell': ('{U}{U}', 'Counter target spell.'),
             'Boomerang': ('{U}{U}', "Return target permanent to its owner's hand."),
             'Twiddle': ('{U}', 'You may tap or untap target artifact, creature, or land.')}

def spec(d, r):
    from recipes import cost_from_oracle
    if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
        or r.get('type_line','').startswith('Token ') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d):
        raise ValueError('fixed mill requires explicit variants and pinned normal nontoken whole text')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'mode','amount','recipient','draw','draw_first','mana_cost','tap','sacrifice'}:
        raise ValueError('explicit mill mode, amount, recipient, draw order and costs required')
    if p['mode'] not in ('spell','activation','entry') or p['recipient'] not in ('any','opponent','self','both'):
        raise ValueError('unsupported mill mode or recipient')
    if type(p['amount']) is not int or p['amount'] not in WORDS or type(p['draw']) is not int or not 0<=p['draw']<=3:
        raise ValueError('bounded fixed mill and draw amounts required')
    if any(type(p[k]) is not bool for k in ('draw_first','tap','sacrifice')) or p['draw_first'] and not p['draw']:
        raise ValueError('explicit boolean costs and meaningful draw order required')
    kind=next((k.lower() for k in ('Creature','Artifact','Enchantment') if k in r.get('type_line','')),None)
    creature='Creature' in r.get('type_line','')
    if set(r.get('keywords',[]))-{'Mill'} or 'Aura' in r.get('type_line','') or 'Equipment' in r.get('type_line',''):
        raise ValueError('additional keywords or attached permanents require dedicated cases')
    clause=({'any':'Target player mills ','opponent':'Target opponent mills ','self':'Mill ','both':'Each player mills '}[p['recipient']]+WORDS[p['amount']]+'.')
    draw='Draw '+WORDS[p['draw']]+'.' if p['draw'] else ''
    lines=([draw,clause] if p['draw_first'] else [clause,draw]) if draw else [clause]
    mana=[0]*6
    if p['mode']=='activation':
        if kind is None:raise ValueError('activation requires standalone permanent')
        mana=cost_from_oracle({'mana_cost':p['mana_cost']}) if p['mana_cost'] else mana
        costs=([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else [])+(['Sacrifice this '+kind] if p['sacrifice'] else [])
        if not costs:raise ValueError('activation requires a cost')
        expected={', '.join(costs)+': '+' '.join(lines)}
    elif p['mode']=='entry':
        if not creature or draw or p['recipient']=='opponent':raise ValueError('simple creature entry mill required')
        expected={f'When {ref} enters{suffix}, {clause[0].lower()+clause[1:]}' for ref in ('this creature',r['name']) for suffix in ('',' the battlefield')}
    else:
        if r.get('type_line') not in ('Instant','Sorcery'):raise ValueError('ordinary instant/sorcery required')
        expected={' '.join(lines),'\n'.join(lines)}
    if p['mode']!='activation' and (p['mana_cost'] or p['tap'] or p['sacrifice']):raise ValueError('nonactivation has no activation costs')
    text=r.get('oracle_text','')
    # These exact reminder sentences do not introduce an additional rule.
    for reminder in (' (They put the top card of their library into their graveyard.)',' (Put the top two cards of your library into your graveyard.)'):
        text=text.replace(reminder,'')
    if text not in expected:raise ValueError('additional mill clauses or changed effect/cost need dedicated cases')
    if creature and any(not re.fullmatch(r'\d+',r.get(k,'')) or not 0<=int(r[k])<=20 for k in ('power','toughness')):
        raise ValueError('bounded numeric source creature required')
    if creature and int(r['toughness'])==0:raise ValueError('source must survive entry')
    return dict(p,family='fixed_mill',activation_mana=mana,mana=cost_from_oracle(r),creature=creature,legendary='Legendary' in r.get('type_line',''))

def variants(p):
    names=['effect','payment','insufficient_mana','wrong_color','empty_graveyard','bare_zones','short_library','exact_library','empty_library',
           'ready_snapshot','pending_snapshot','resolved_snapshot','cleanup_once','two_copies']
    if p['recipient']=='any':names+=['self_target']
    if p['mode']=='activation':
        names+=['tapped','sick','opposing_turn']
        if not p['sacrifice']:names+=['source_destroyed_pending','source_bounced_pending']+(['repeat_denied','untap_repeat'] if p['tap'] else ['stacked_activations'])
        names+=['cast','cast_payment','fresh_activation']
    else:names+=['countered']
    if not any((p['activation_mana'] if p['mode']=='activation' else p['mana'])[1:]):names.remove('wrong_color')
    if not sum(p['activation_mana'] if p['mode']=='activation' else p['mana']):names.remove('insufficient_mana')
    # A cantrip on an empty library loses the game; that needs a separate loss fixture.
    if p['draw']:
        names.remove('empty_library')
        if p['recipient']=='self':names.remove('short_library');names.remove('exact_library')
    if p['legendary']:names.remove('two_copies')
    # Player 1's legacy AI validator constrains targets to its strategic
    # preference. Self-target legality is exercised with the human controller;
    # never treat an AI policy refusal as a rules failure.
    return [v+'_'+str(owner) for owner in (0,1) for v in names if not (owner==1 and v=='self_target')]

def validate_witnesses(oracle,registry):
    for name,fields in WITNESSES.items():
        if name not in registry or tuple(oracle.get(name,{}).get(k) for k in ('mana_cost','oracle_text'))!=fields:
            raise ValueError('fixed mill witness changed: '+name)

def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step,observe
    result=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);activation=p['mode']=='activation'
        targets=[owner] if p['recipient']=='self' or v=='self_target' else [0,1] if p['recipient']=='both' else [1-owner]
        bare=v=='bare_zones';hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare or v=='empty_graveyard' else list(GRAVE) for _ in (0,1)]
        exiles=[[] if bare else list(EXILE) for _ in (0,1)]
        # Long, recognizable ordering with duplicates exercises exact transfer counts.
        libs=[list(LIBRARY)*4 for _ in (0,1)]
        for player in targets:
            if v in ('short_library','exact_library','empty_library'):
                libs[player]=libs[player][:max(0,p['amount']-1) if v=='short_library' else p['amount'] if v=='exact_library' else 0]
        mana=[[10]*6,[10]*6];setup=[];pre=[];actions=[];entities={};stack=0
        casting=not activation or v in ('cast','cast_payment','fresh_activation')
        def add(nm,ref,player,zone='battlefield',state=None):
            item=card(nm,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='hand':hands[player].append(nm)
            else:entities[ref]=dict(player=player,name=nm,alive=True,tapped=bool(state and 'tapped' in state))
        for player in (0,1):
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                setup += [card(nm,f'{zone}_{player}_{i}',player,zone) for i,nm in enumerate(names)]
        add(name,'source',owner,'hand' if casting else 'battlefield',['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
        if v=='two_copies':add(name,'second',owner,'battlefield' if activation else 'hand')
        if v in ('countered','source_bounced_pending'):add('Counterspell' if v=='countered' else 'Boomerang','response',0 if v=='countered' else 1-owner,'hand')
        if v=='untap_repeat':add('Twiddle','untapper',owner,'hand')
        pre += [step('turn',player=1-owner if v=='opposing_turn' else owner,value=0)]
        def pools(player,pool):return [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(pool)]
        cost=list(p['activation_mana'] if activation else p['mana'])
        if v in ('payment','cast_payment'):mana[owner]=list(p['mana'] if v=='cast_payment' else cost)
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(cost);c=next(c for c in range(1,6) if cost[c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        if v in ('two_copies','stacked_activations','untap_repeat'):mana[owner][0]=max(10,2*cost[0])
        for player in (0,1):pre+=pools(player,mana[player])
        pre.append(step('refresh'))
        def zones(pending_spell=False):
            checks=[]
            for player in (0,1):
                for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                    # The engine's hand inspection includes a pending spell's
                    # active slot. Assert semantic hand contents after resolution.
                    if pending_spell and zone=='hand':continue
                    checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone in ('library','graveyard') else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
                if not pending_spell:checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
            return checks
        # Target-opponent spells automatically select the only opponent. There
        # is no player-selection dialog to script for that forced choice.
        def choice():return [step('choose_player',player=owner,value=targets[0])] if p['recipient']=='any' else []
        def pending(ref='source'):
            nonlocal stack
            seq=[]
            if activation:seq += [step('require_can_activate',ref=ref,value=1)]
            if p['mode']!='entry':seq+=choice()
            seq += [step('activate' if activation else 'cast',ref=ref,value=1)]
            if p['mode']!='entry':seq.append(step('assert_choices_consumed'))
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)];stack+=1
            if not activation:hands[owner].remove(name)
            elif p['sacrifice']:entities[ref]['alive']=False;graves[owner].append(name)
            elif p['tap']:entities[ref]['tapped']=True
            if activation:
                seq.append(step('require_card',ref=ref,property='in_play',value=int(not p['sacrifice'])))
                if p['tap'] and not p['sacrifice']:seq.append(step('require_card',ref=ref,property='tapped',value=1))
            seq += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
            return seq+[step('assert_stack',value=stack)]+zones(pending_spell=not activation)
        def effect():
            if p['draw_first']:hands[owner].extend(libs[owner][:p['draw']]);del libs[owner][:p['draw']]
            for player in targets:graves[player].extend(libs[player][:p['amount']]);del libs[player][:p['amount']]
            if not p['draw_first']:hands[owner].extend(libs[owner][:p['draw']]);del libs[owner][:p['draw']]
        def resolved(ref='source'):
            nonlocal stack
            seq=choice() if p['mode']=='entry' else []
            seq += [step('resolve'),step('assert_choices_consumed')];stack=0;effect()
            if p['mode']=='spell':graves[owner].append(name)
            elif p['mode']=='entry':entities[ref]=dict(player=owner,name=name,alive=True,tapped=False)
            return seq
        ready=len(actions);pending_cut=0
        illegal=v in ('insufficient_mana','wrong_color') or activation and p['tap'] and (v=='tapped' or v=='sick' and p['creature'])
        if activation and casting:
            actions += [step('cast',ref='source',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
            hands[owner].remove(name);entities['source']=dict(player=owner,name=name,alive=True,tapped=False);mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])]
            if v=='fresh_activation':
                mana=[[10]*6,[10]*6]
                for player in (0,1):actions+=pools(player,mana[player])
                actions.append(step('refresh'));illegal=p['creature'] and p['tap']
        if v in ('cast','cast_payment') and activation:pass
        elif illegal:
            if activation:actions.append(step('require_can_activate',ref='source',value=0))
            actions += [step('activate' if activation else 'cast',ref='source',value=0),step('assert_choices_consumed')]
        elif v=='countered':
            # Counter through the real response window opened by casting.
            # The counter helper selects the top spell; a countered cast returns 0.
            actions += (choice() if p['mode']=='spell' else [])+[step('choose_priority',ref='response'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)];mana[0][2]-=2
            hands[owner].remove(name);hands[0].remove('Counterspell');graves[owner].append(name);graves[0].append('Counterspell')
        else:
            actions+=pending();pending_cut=len(actions)
            if v=='stacked_activations':actions+=pending()
            if v=='source_destroyed_pending':actions.append(step('destroy',ref='source'));entities['source']['alive']=False;graves[owner].append(name)
            if v=='source_bounced_pending':
                actions += [step('choose_target',player=1-owner,ref='source'),step('cast',ref='response',value=1)]
                mana[1-owner][2]-=2;hands[1-owner].remove('Boomerang');graves[1-owner].append('Boomerang');hands[owner].append(name);entities['source']['alive']=False
            actions+=resolved()
            if v=='stacked_activations':effect()
            if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
            if v=='untap_repeat':
                actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='untapper',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
                mana[owner][2]-=1;hands[owner].remove('Twiddle');graves[owner].append('Twiddle');entities['source']['tapped']=False
            if v in ('two_copies','untap_repeat'):actions+=pending('second' if v=='two_copies' else 'source')+resolved('second' if v=='two_copies' else 'source')
            if v=='cleanup_once':actions+=zones()+[step('cleanup')]
        checks=zones()+[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=20),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
            if v!='cleanup_once':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
        for ref,e in entities.items():
            checks.append(observe(ref,'in_play',int(e['alive'])))
            if e['alive']:checks.append(observe(ref,'tapped',int(e['tapped'])))
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot'):
            cut=ready if v=='ready_snapshot' else pending_cut
            steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'FixedMillRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='fixed_mill',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['empty-library draw loss','source creature combat characteristics'])))
    return result
