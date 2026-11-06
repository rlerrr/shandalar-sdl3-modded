"""Ordinary mana lands/artifacts with explicit costs, outputs and entry clauses."""
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from mana_creature_recipes import output_pool, ANY_COLOR, WITNESSES as CREATURES, HULK

SPELLS = {
    'Twiddle': ('{U}', 'You may tap or untap target artifact, creature, or land.'),
    'Boomerang': ('{U}{U}', "Return target permanent to its owner's hand."),
    'Negate': ('{1}{U}', 'Counter target noncreature spell.'),
    'Suppression Field': ('{1}{W}', "Activated abilities cost {2} more to activate unless they're mana abilities."),
}
OUTPUT_MODES = ('production', 'payment', 'repeat_denied', 'spend_generic', 'ready_snapshot', 'untap_repeat')

def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' in record.get('type_line', '') or not any(t in record.get('type_line', '') for t in ('Artifact','Land'))
            or declaration.get('oracle_text') != record.get('oracle_text') or 'variants' not in declaration):
        raise ValueError('mana permanent requires pinned normal noncreature land/artifact and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p,dict) or set(p) != {'activation_cost','mana_outputs','production','enters_tapped','entry_life','indestructible'}:
        raise ValueError('explicit mana cost, outputs, production and entry parameters required')
    if type(p['enters_tapped']) is not bool or type(p['indestructible']) is not bool or type(p['entry_life']) is not int or p['entry_life'] not in (0,1):
        raise ValueError('reviewed boolean entry/indestructibility and zero/one life required')
    outputs=p['mana_outputs'];form=p['production'];cost=p['activation_cost']
    if not isinstance(outputs,list) or not outputs or any(not isinstance(o,str) for o in outputs) or len(set(outputs))!=len(outputs):
        raise ValueError('unique explicit mana outputs required')
    pools=[output_pool(o) for o in outputs]
    if form=='fixed' and len(outputs)==1:wording=outputs[0]
    elif form=='choice' and 2<=len(outputs)<=3 and all(sum(o)==1 for o in pools):
        wording=' or '.join(outputs) if len(outputs)==2 else ', '.join(outputs[:-1])+', or '+outputs[-1]
    elif form=='any_color' and outputs==ANY_COLOR:wording='one mana of any color'
    elif form=='any_one_color' and len(outputs)==5 and all(outputs[i]==ANY_COLOR[i]*sum(pools[0]) for i in range(5)) and sum(pools[0]) in (2,3):
        wording={2:'two',3:'three'}[sum(pools[0])]+' mana of any one color'
    else:raise ValueError('unreviewed explicit production form')
    if not isinstance(cost,str) or not re.fullmatch(r'(?:\{[1-3]\}, )?\{T\}',cost):
        raise ValueError('ordinary optional generic input and tap cost required')
    clause=cost+': Add '+wording+'.'
    lines=record['oracle_text'].splitlines();remaining=[];seen=Counter();land='Land' in record['type_line'];kind='land' if land else 'artifact'
    for line in lines:
        if line==clause or land and line=='('+clause+')':seen['mana']+=1
        elif line==f'This {kind} enters tapped.':seen['tapped']+=1
        elif land and line=='When this land enters, you gain 1 life.':seen['life']+=1
        elif line in ('Indestructible','Indestructible (Effects that say "destroy" don\'t destroy this artifact.)'):
            seen['indestructible']+=1
        elif land and re.fullmatch(r'Cycling \{[1-3BUGRW]\}(?: \([^\n]+\))?',line):remaining.append(line)
        else:raise ValueError('additional mana, entry, static or triggered clause needs dedicated cases')
    if seen != Counter(mana=1,tapped=int(p['enters_tapped']),life=p['entry_life'],indestructible=int(p['indestructible'])):
        raise ValueError('explicit entry clauses must match complete pinned Oracle text')
    expected_keys=({'Indestructible'} if p['indestructible'] else set())|({'Cycling'} if remaining else set())
    if set(record.get('keywords',[]))!=expected_keys:raise ValueError('unreviewed mana permanent keyword')
    cast_mana=None if land else [0]*6 if record.get('mana_cost')=='{0}' else cost_from_oracle(record)
    input_mana=[int(cost[1]) if ',' in cost else 0,0,0,0,0,0]
    return dict(p,family='permanent_mana',land=land,cast_mana=cast_mana,activation_mana=input_mana,
                output_pools=pools,choice=form!='fixed',untested_rules=remaining)

def variants(p):
    common=['entry','tapped','sick','opposing_turn','combat','bare_zones','two_sources','bounce','destroy','tax','resolved_snapshot']
    if p['activation_mana'][0]:common += ['insufficient_input']
    if not p['land']:
        common += ['fresh_cast','pending_cast_snapshot','countered']
        if sum(p['cast_mana']):common += ['insufficient_cast_mana']
    result=[v+'_'+str(owner) for owner in (0,1) for v in common]
    for owner in (0,1):
        for index,pool in enumerate(p['output_pools']):
            modes=list(OUTPUT_MODES)+(['spend_colored'] if not pool[0] else [])
            result += [v+'_'+str(index)+'_'+str(owner) for v in modes]
    return result

def validate_witnesses(oracle,registry):
    for name,fields in SPELLS.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=fields:raise ValueError('mana permanent witness changed: '+name)
    for cost,(name,mana,power,toughness,text) in list(CREATURES.items())[:5]+[('generic',HULK)]:
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('power'),r.get('toughness'),r.get('oracle_text'))!=(mana,str(power),str(toughness),text):
            raise ValueError('mana spending witness changed: '+name)

def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe
    result=[]
    for variant in selected:
        bits=variant.rsplit('_',2);indexed=len(bits)==3 and bits[1].isdigit()
        if indexed:v,index,owner=bits[0],int(bits[1]),int(bits[2])
        else:v,owner=variant.rsplit('_',1);owner=int(owner);index=0
        output=p['output_pools'][index];input_mana=p['activation_mana'];casting=v in ('fresh_cast','pending_cast_snapshot','countered','insufficient_cast_mana')
        fixture='empty_main' if v=='bare_zones' else 'stocked_zones'
        hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];libs=[list(LIBRARY),list(LIBRARY)];exile=[list(EXILE),list(EXILE)]
        if v=='bare_zones':hands=[[],[]];graves=[[],[]];libs=[[],[]];exile=[[],[]]
        battle=[0,0];life=[20,20];mana=[[10]*6,[10]*6];setup=[];pre=[];actions=[];checks=[];source_alive=True;source_tapped=False
        setup.append(card(name,'source',owner,'hand' if casting else 'battlefield'))
        if casting:hands[owner].append(name)
        else:battle[owner]+=1;life[owner]+=p['entry_life'];source_tapped=p['enters_tapped']
        if v=='sick':setup[0]['state']=['sick']
        if v=='tapped':setup[0]['state']=['tapped'];source_tapped=True
        pre += [step('turn',player=1-owner if v=='opposing_turn' else owner,value=0)]
        def pool_steps(player,pool):return [step('set_mana',player=player,color=c,value=n) for c,n in enumerate(pool)]
        def untap(ref,stage):
            setup.append(card('Twiddle',stage,owner,'hand'))
            graves[owner].append('Twiddle');mana[owner][2]-=1
            return [step('choose_target',player=owner,ref=ref),step('cast',ref=stage,value=1),step('assert_stack',value=1),
                    step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed'),observe(ref,'tapped',0)]
        pre += [step('require_card',ref='source',property='in_hand' if casting else 'in_play',value=1)]
        if not casting and source_tapped and v not in ('entry','tapped'):
            pre+=untap('source','initial_untap');source_tapped=False
        if v=='two_sources':
            setup.append(card(name,'second',owner));battle[owner]+=1;life[owner]+=p['entry_life']
            if p['enters_tapped']:pre+=untap('second','second_untap')
        if v=='combat':
            setup.append(card('Savannah Lions','attacker',1-owner));battle[1-owner]+=1
            pre += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers')]
        if v=='tax':
            setup.append(card('Suppression Field','field',1-owner,'hand'));battle[1-owner]+=1
            pre += [step('cast',ref='field',value=1),step('resolve'),step('assert_choices_consumed')]
        # Pay legal setup spells, then install the explicit scenario's mana pools.
        mana=[[10]*6,[10]*6]
        if v=='payment':mana[owner]=list(input_mana)
        if v=='insufficient_input':mana[owner]=[0]*6
        if v in ('spend_generic','spend_colored'):
            mana[owner]=list(input_mana)
            if v=='spend_generic':
                mana[owner][0]+=6-sum(output);setup.append(card(HULK[0],'spend_0',owner,'hand'))
            else:
                count=0
                for color in range(1,6):
                    for _ in range(output[color]):
                        setup.append(card(CREATURES['{'+ 'CBUGRW'[color] +'}'][0],'spend_'+str(count),owner,'hand'));count+=1
        if casting:
            mana[owner]=[a+b for a,b in zip(p['cast_mana'],input_mana)]
            if p['enters_tapped'] and v not in ('countered','insufficient_cast_mana'):mana[owner][2]+=1
            if v=='insufficient_cast_mana':mana[owner]=[0]*6
            if v=='countered' and owner==0:mana[0][0]+=1;mana[0][2]+=1
        pre += pool_steps(0,mana[0])+pool_steps(1,mana[1])
        if v=='insufficient_input':
            # Raw fixture overrides bypass normal gameplay's mana recount.
            # Recount after emptying the pool so prior producible mana is not
            # mistaken for a way to bootstrap a filtering source's input cost.
            pre.append(step('refresh'))
        def activate(ref='source'):
            seq=[step('require_can_activate',ref=ref,value=1)]
            if p['choice']:seq.append(step('choose_color',player=owner,value=next(c for c,n in enumerate(output) if n)))
            seq += [step('activate',ref=ref,value=1),step('assert_stack',value=0),step('assert_choices_consumed'),observe(ref,'tapped',1)]
            mana[owner]=[a-b+c for a,b,c in zip(mana[owner],input_mana,output)]
            seq += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
            return seq
        if casting and v!='insufficient_cast_mana':
            actions += ([step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('assert_stack',value=0)] if v=='countered' else [step('cast',ref='source',value=1),step('assert_stack',value=1)])
            hands[owner].remove(name);mana[owner]=[a-b for a,b in zip(mana[owner],p['cast_mana'])]
            if v=='countered':
                setup.append(card('Negate','counter',0,'hand'))
                # The cast itself opens the game's real counterspell response window.
                mana[0][0]-=1;mana[0][2]-=1;graves[0].append('Negate');graves[owner].append(name);source_alive=False
            cut=len(actions)
            actions += [step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed'),step('refresh')]
            if source_alive:
                battle[owner]+=1;life[owner]+=p['entry_life'];source_tapped=p['enters_tapped']
                actions += [step('require_card',ref='source',property='in_play',value=1),observe('source','tapped',int(source_tapped))]
                if source_tapped:actions+=untap('source','fresh_untap');source_tapped=False
        if v=='insufficient_cast_mana':
            actions += [step('cast',ref='source',value=0),step('assert_choices_consumed')];source_alive=False
        elif v in ('entry','tapped','insufficient_input'):
            actions += [step('require_can_activate' if v=='insufficient_input' else 'can_activate',ref='source',value=int(v=='entry' and not source_tapped))]
            if v!='entry':actions += [step('activate',ref='source',value=0),step('assert_choices_consumed')]
        elif v=='countered':pass
        else:
            ready=len(actions);actions+=activate();source_tapped=True
            if v=='repeat_denied':actions += [step('can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
            if v=='two_sources':actions+=activate('second');checks.append(observe('second','tapped',1))
            if v=='untap_repeat':actions+=untap('source','repeat_untap')+activate()
            if v in ('spend_generic','spend_colored'):
                if v=='spend_generic':to_cast=[HULK[0]]
                else:to_cast=[CREATURES['{'+ 'CBUGRW'[c] +'}'][0] for c in range(1,6) for _ in range(output[c])]
                for i,nm in enumerate(to_cast):
                    r=oracle[nm];pool=[6,0,0,0,0,0] if v=='spend_generic' else [int(c==next(c for c in range(1,6) if '{'+'CBUGRW'[c]+'}'==r['mana_cost'])) for c in range(6)]
                    if v=='spend_generic' and owner==0:actions += [step('choose_mana',player=owner,color=c) for c in range(1,6) for _ in range(output[c])]
                    actions += [step('cast',ref='spend_'+str(i),value=1),step('assert_stack',value=1),step('resolve'),step('require_card',ref='spend_'+str(i),property='in_play',value=1)]
                    if v=='spend_generic':mana[owner]=[0]*6
                    else:mana[owner]=[a-b for a,b in zip(mana[owner],pool)]
                    battle[owner]+=1
            if v=='bounce':
                setup.append(card('Boomerang','bounce',0,'hand'));graves[0].append('Boomerang');mana[0][2]-=2
                actions += [step('choose_target',player=0,ref='source'),step('cast',ref='bounce',value=1),step('resolve')]
                battle[owner]-=1;hands[owner].append(name);source_alive=False
            if v=='destroy':
                actions.append(step('destroy',ref='source'))
                if not p['indestructible']:battle[owner]-=1;graves[owner].append(name);source_alive=False
        checks += [step('assert_stack',value=0),step('assert_choices_consumed')]
        if source_alive:checks += [observe('source','in_play',1),observe('source','tapped',int(source_tapped))]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',libs[player]),('exile',exile[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(dict.fromkeys(names) if zone!='library' else names)]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_cast_snapshot'):
            cut=ready if v=='ready_snapshot' else cut
            steps=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        result.append(dict(name=f'PermanentManaRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='permanent_mana',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_rules=p['untested_rules'],untested_components=['land play permissions','cycling','multiplayer'])))
    return result


# Wave-six opt-in branches keep every old declaration on its original path.
_wave6_original_spec = spec
_wave6_original_variants = variants
_wave6_original_expand = expand
_wave6_original_validate = validate_witnesses

def spec(d, o):
    from mana_spell_extension_recipes import applies, spec as extension_spec
    return extension_spec(d, o) if applies(d) else _wave6_original_spec(d, o)

def variants(p):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import variants as extension_variants
        return extension_variants(p)
    return _wave6_original_variants(p)

def validate_witnesses(o, registry):
    _wave6_original_validate(o, registry)

def expand(name, csvid, d, p, selected, registry, o):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import expand as extension_expand
        return extension_expand(name, csvid, d, p, selected, registry, o)
    return _wave6_original_expand(name, csvid, d, p, selected, registry, o)
