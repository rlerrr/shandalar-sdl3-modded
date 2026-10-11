"""Fixed self draw/life abilities, including explicit tap, life and self-sacrifice costs."""
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from mana_creature_recipes import output_pool

SPELLS = {
    'Twiddle': ('{U}', 'You may tap or untap target artifact, creature, or land.'),
    'Boomerang': ('{U}{U}', "Return target permanent to its owner's hand."),
    'Suppression Field': ('{1}{W}', "Activated abilities cost {2} more to activate unless they're mana abilities."),
}

def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line','').startswith('Token ') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('value activation requires pinned normal nontoken whole text and explicit variants')
    types=record.get('type_line','');kind=next((t.lower() for t in ('Creature','Artifact','Enchantment','Land') if t in types),None)
    if kind is None or 'Aura' in types or 'Equipment' in types:raise ValueError('ordinary standalone permanent required')
    p=declaration.get('params')
    if not isinstance(p,dict) or set(p)!={'effect','amount','mana_cost','tap','sacrifice','pay_life','mana_outputs'}:
        raise ValueError('explicit value, mana, tap, sacrifice, life and alternate mana parameters required')
    if p['effect'] not in ('draw','gain_life') or type(p['amount']) is not int or not 1<=p['amount']<=(3 if p['effect']=='draw' else 10):
        raise ValueError('bounded fixed draw/life effect required')
    if any(type(p[k]) is not bool for k in ('tap','sacrifice')) or type(p['pay_life']) is not int or not 0<=p['pay_life']<=5:
        raise ValueError('boolean tap/sacrifice and bounded life payment required')
    mana=[0]*6 if p['mana_cost']=='' else cost_from_oracle({'mana_cost':p['mana_cost']})
    pieces=([p['mana_cost']] if p['mana_cost'] else [])+(['{T}'] if p['tap'] else [])
    if p['pay_life']:pieces.append(f"Pay {p['pay_life']} life")
    if p['sacrifice']:pieces.append('Sacrifice this '+kind)
    if not pieces:raise ValueError('nonempty activation cost required')
    effect=('Draw '+{1:'a card',2:'two cards',3:'three cards'}[p['amount']]+'.' if p['effect']=='draw' else f"You gain {p['amount']} life.")
    clause=', '.join(pieces)+': '+effect
    outputs=p['mana_outputs']
    if not isinstance(outputs,list) or len(outputs)>3 or any(not isinstance(o,str) for o in outputs) or len(set(outputs))!=len(outputs):
        raise ValueError('zero to three unique explicit ordinary alternate mana outputs required')
    pools=[output_pool(o) for o in outputs]
    if len(outputs)>1 and any(sum(pool)!=1 for pool in pools):raise ValueError('alternate choices must produce one mana')
    wording=outputs[0] if len(outputs)==1 else ' or '.join(outputs) if len(outputs)==2 else ', '.join(outputs[:-1])+', or '+outputs[-1] if outputs else ''
    expected=([f'{{T}}: Add {wording}.'] if outputs else [])+[clause]
    if record.get('oracle_text','').splitlines()!=expected or record.get('keywords'):
        raise ValueError('additional clauses, restrictions, keywords or nonstandard costs need dedicated cases')
    creature='Creature' in types;land='Land' in types
    if creature and any(not re.fullmatch(r'\d+',record.get(k,'')) or not 0<=int(record[k])<=20 for k in ('power','toughness')):
        raise ValueError('numeric bounded creature characteristics required')
    if creature and int(record['toughness'])==0:raise ValueError('source must survive fixture entry')
    casting=None if land else [0]*6 if record.get('mana_cost')=='{0}' else cost_from_oracle(record)
    return dict(p,family='value_activation',activation_mana=mana,mana=casting,creature=creature,land=land,output_pools=pools)

def variants(p):
    common=['value','payment','tapped','sick','opposing_turn','combat','two_sources','bare_zones',
            'ready_snapshot','pending_snapshot','resolved_snapshot','tax']
    if not p['land']:
        common+=['cast','cast_payment','fresh_activation']
        if sum(p['mana']):common+=['insufficient_cast_mana']
    if sum(p['activation_mana']):common+=['insufficient_mana','red_payment'] if not any(p['activation_mana'][1:]) else ['insufficient_mana','wrong_color']
    if p['pay_life']:common+=['insufficient_life','low_life']
    if not p['sacrifice']:common+=['source_bounced_pending','source_destroyed_pending','cleanup']+(['repeat_denied','untap_repeat'] if p['tap'] else ['stacked_activations'])
    else:common+=['second_copy_pending']
    result=[v+'_'+str(owner) for owner in (0,1) for v in common]
    result += [f'mana_{index}_{owner}' for owner in (0,1) for index in range(len(p['mana_outputs']))]
    return result

def validate_witnesses(oracle,registry):
    for name,fields in SPELLS.items():
        r=oracle.get(name,{})
        if name not in registry or r.get('layout')!='normal' or (r.get('mana_cost'),r.get('oracle_text'))!=fields:
            raise ValueError('value activation witness changed: '+name)
    for name,pt in (('Grizzly Bears',('2','2')),('Hill Giant',('3','3'))):
        r=oracle.get(name,{})
        if name not in registry or (r.get('power'),r.get('toughness'))!=pt or r.get('oracle_text')!='':
            raise ValueError('value activation combat witness changed: '+name)

def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe
    result=[]
    for variant in selected:
        bits=variant.rsplit('_',2);indexed=len(bits)==3 and bits[1].isdigit()
        if indexed:v,index,owner=bits[0],int(bits[1]),int(bits[2])
        else:v,owner=variant.rsplit('_',1);owner=int(owner);index=0
        bare=v=='bare_zones';fixture='empty_main' if bare else 'stocked_zones'
        hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
        libs=[list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
        mana=[[10]*6,[10]*6];life=[20,20];entities={};setup=[];pre=[];actions=[];checks=[]
        def add(nm,ref,player,zone='battlefield',state=None):
            item=card(nm,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='hand':hands[player].append(nm)
            else:entities[ref]=dict(name=nm,player=player,alive=True,tapped=bool(state and 'tapped' in state))
        casting=v in ('cast','cast_payment','insufficient_cast_mana','fresh_activation')
        add(name,'source',owner,'hand' if casting else 'battlefield', ['tapped'] if v=='tapped' else ['sick'] if v=='sick' else None)
        if bare:
            # Empty hand/graveyard/exile, but deliberate ordered libraries to draw from.
            for player in (0,1):
                setup += [card(nm,f'library_{player}_{i}',player,'library') for i,nm in enumerate(LIBRARY)]
        if v in ('two_sources','second_copy_pending'):add(name,'second',owner)
        if v=='source_bounced_pending':add('Boomerang','response',0,'hand')
        if v=='untap_repeat':add('Twiddle','untapper',owner,'hand')
        pre += [step('turn',player=1-owner if v=='opposing_turn' else owner,value=0)]
        if v=='combat':
            add('Grizzly Bears','attacker',1-owner);add('Hill Giant','blocker',owner)
            pre += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers')]
            entities['attacker']['tapped']=True
        def pools(player,pool):return [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(pool)]
        if v=='tax':
            add('Suppression Field','taxer',1-owner,'hand')
            pre += [step('turn',player=1-owner,value=0),step('cast',ref='taxer',value=1),step('resolve'),step('assert_choices_consumed'),step('turn',player=owner,value=0)]
            hands[1-owner].remove('Suppression Field');entities['taxer']=dict(name='Suppression Field',player=1-owner,alive=True,tapped=False)
        if casting:
            cm=list(p['mana']) if v=='cast_payment' else [0]*6 if v=='insufficient_cast_mana' else [10]*6
            pre+=pools(owner,cm);mana[owner]=cm
            pre += [step('cast',ref='source',value=int(v!='insufficient_cast_mana')),step('assert_choices_consumed')]
            if v!='insufficient_cast_mana':
                pre += [step('assert_stack',value=1),step('resolve'),step('refresh'),step('require_card',ref='source',property='in_play',value=1)]
                hands[owner].remove(name);entities['source']=dict(name=name,player=owner,alive=True,tapped=False);mana[owner]=[a-b for a,b in zip(cm,p['mana'])]
        input_mana=list(p['activation_mana']);input_mana[0]+=2*int(v=='tax')
        # Setup spells have been paid normally; use isolated pools for the ability.
        if v not in ('cast_payment','insufficient_cast_mana'):mana=[[10]*6,[10]*6]
        if v in ('payment','tax'):mana[owner]=list(input_mana)
        if v in ('two_sources','second_copy_pending','stacked_activations','untap_repeat'):
            # Two generic payments can exceed the shared fixture's ten mana.
            # Supply both costs rather than accidentally invoking color payment.
            mana[owner][0]=max(mana[owner][0],2*input_mana[0])
        if v in ('insufficient_mana','mana'):mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(input_mana);c=next(c for c in range(1,6) if input_mana[c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        if v=='red_payment':mana[owner]=[0,0,0,0,sum(input_mana),0]
        if v in ('insufficient_life','low_life'):
            life[owner]=p['pay_life']-1 if v=='insufficient_life' else p['pay_life']+1
            pre.append(step('set_life',player=owner,value=life[owner]))
        for player in (0,1):pre+=pools(player,mana[player])
        pre += [step('refresh')]
        if v!='insufficient_cast_mana':pre += [step('require_card',ref='source',property='in_play',value=1)]
        legal=(v not in ('insufficient_mana','wrong_color','insufficient_life','insufficient_cast_mana')
               and not (v=='tapped' and p['tap']) and not (v in ('sick','fresh_activation') and p['creature'] and p['tap']))
        def pending(ref='source'):
            seq=[step('require_can_activate',ref=ref,value=1)]
            if p['mana_outputs']:seq.append(step('choose_dialog',player=owner,value=1))
            if v=='red_payment' and owner==0:seq += [step('choose_mana',player=owner,color=4) for _ in range(sum(input_mana))]
            seq += [step('activate',ref=ref,value=1),step('assert_choices_consumed')]
            mana[owner]=[a-b for a,b in zip(mana[owner],[0,0,0,0,sum(input_mana),0] if v=='red_payment' else input_mana)]
            life[owner]-=p['pay_life']
            if p['sacrifice']:entities[ref]['alive']=False;graves[owner].append(name)
            elif p['tap']:entities[ref]['tapped']=True
            # Costs precede the effect. Do not proceed if activation went astray.
            seq += [step('require_card',ref=ref,property='in_play',value=int(not p['sacrifice']))]
            if p['tap'] and not p['sacrifice']:seq.append(step('require_card',ref=ref,property='tapped',value=1))
            seq += [step('assert_life',player=owner,value=life[owner]),step('assert_zone',player=owner,zone='hand',value=len(hands[owner])),step('assert_zone',player=owner,zone='library',value=len(libs[owner]))]
            return seq
        def apply_value():
            if p['effect']=='draw':hands[owner].extend(libs[owner][:p['amount']]);del libs[owner][:p['amount']]
            else:life[owner]+=p['amount']
        ready=0;pending_cut=0
        if v=='insufficient_cast_mana':pass
        elif v=='mana':
            # With no input mana these enrolled sources expose only their mana
            # ability; no mode-selection dialog is presented.
            actions += [step('require_can_activate',ref='source',value=1)]
            if len(p['mana_outputs'])>1:actions.append(step('choose_color',player=owner,value=next(c for c,n in enumerate(p['output_pools'][index]) if n)))
            actions += [step('activate',ref='source',value=1),step('assert_choices_consumed'),step('assert_stack',value=0)]
            entities['source']['tapped']=True;mana[owner]=list(p['output_pools'][index])
        elif not legal:
            fallback=bool(p['mana_outputs']) and v in ('insufficient_mana','wrong_color','insufficient_life')
            actions += [step('require_can_activate',ref='source',value=int(fallback))]
            if fallback:
                # Full-card eligibility includes the affordable mana branch.
                # Use that legal alternative; never force a disabled value choice.
                # A nonempty wrong-color pool can make the source's cached mana
                # availability expose a mode menu. Select the legal mana mode;
                # the completely empty pool routes there without a menu.
                if v=='wrong_color':actions.append(step('choose_dialog',player=owner,value=0))
                if len(p['mana_outputs'])>1:actions.append(step('choose_color',player=owner,value=next(c for c,n in enumerate(p['output_pools'][0]) if n)))
                actions += [step('activate',ref='source',value=1)]
                entities['source']['tapped']=True;mana[owner]=[a+b for a,b in zip(mana[owner],p['output_pools'][0])]
            else:actions += [step('activate',ref='source',value=0)]
            actions += [step('assert_choices_consumed'),step('assert_stack',value=0)]
        elif v in ('cast','cast_payment'):pass
        else:
            ready=len(actions);actions+=pending()+[step('assert_stack',value=1)];pending_cut=len(actions)
            if v in ('two_sources','second_copy_pending','stacked_activations'):
                actions += pending('source' if v=='stacked_activations' else 'second')+[step('assert_stack',value=2)]
            if v=='source_bounced_pending':
                actions += [step('choose_target',player=0,ref='source'),step('cast',ref='response',value=1),step('assert_stack',value=2),step('resolve'),step('assert_choices_consumed')]
                hands[0].remove('Boomerang');graves[0].append('Boomerang');hands[owner].append(name);entities['source']['alive']=False;mana[0][2]-=2
            if v=='source_destroyed_pending':
                actions += [step('destroy',ref='source')];entities['source']['alive']=False;graves[owner].append(name)
            actions += [step('resolve'),step('assert_choices_consumed')];apply_value()
            if v in ('two_sources','second_copy_pending','stacked_activations'):actions += [step('resolve')];apply_value()
            if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
            if v=='untap_repeat':
                hands[owner].remove('Twiddle');graves[owner].append('Twiddle');mana[owner][2]-=1
                actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='untapper',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed'),step('refresh'),step('require_card',ref='source',property='tapped',value=0)]
                entities['source']['tapped']=False;actions += pending()+[step('resolve')];apply_value()
            if v=='cleanup':
                actions += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
                actions += [step('cleanup')]
        checks += [step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            checks.append(observe(ref,'in_play',int(e['alive'])))
            if e['alive']:checks.append(observe(ref,'tapped',int(e['tapped'])))
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]
            # cleanup dispatches cleanup effects; it does not advance through a
            # priority/phase boundary or model mana-pool emptying.
            if v!='cleanup':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exiles[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names if zone=='library' else dict.fromkeys(names))]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=n) for nm,n in Counter(hands[player]+graves[player]).items()]
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot'):
            cut=ready if v=='ready_snapshot' else pending_cut
            steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'ValueActivationRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=steps,
            _recipe=dict(family='value_activation',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=['empty-library loss','land play permissions','creature combat characteristics'])))
    return result
