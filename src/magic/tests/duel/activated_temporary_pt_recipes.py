"""Fixed signed temporary PT from complete Oracle activation clauses."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from permanent_value_activation_recipes import mana, parse_cost

FAMILY='activated_temporary_pt'
DATA=Path(__file__).parent/'data/activated_temporary_pt_wave8_profiles.json'


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def parse(record,ability):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line','').startswith('Token ')):
        raise ValueError('whole normal implemented permanent required')
    lines=record['oracle_text'].splitlines()
    if type(ability) is not int or not 0<=ability<len(lines) or ': ' not in lines[ability]:raise ValueError('explicit printed activated line required')
    raw,effect=lines[ability].split(': ',1)
    for nm in sorted({record['name'],record['name'].split(',')[0]},key=len,reverse=True):effect=effect.replace(nm,'this creature')
    m=re.fullmatch(r'(This creature|Target creature) gets ([+-]\d+)/([+-]\d+) until end of turn\.',effect)
    if not m:raise ValueError('complete fixed signed temporary PT clause required')
    c=parse_cost(raw)
    if (c['x'] or c['pay_life'] or 'counter' in c or 'loyalty' in c or c['discard'] not in (0,1)
            or c['sacrifice'] not in (None,'self','creature','artifact','land')):raise ValueError('unpiloted temporary cost')
    creature='Creature' in record['type_line']
    if creature and (any(not re.fullmatch(r'-?\d+',record.get(k,'')) for k in ('power','toughness')) or int(record['toughness'])<=0):raise ValueError('signed numeric source base with positive toughness required')
    if m[1]=='This creature' and not creature:raise ValueError('self creature effect requires creature source')
    alternative=-1;siblings=[]
    for i,line in enumerate(lines):
        if i==ability or ': ' not in line:continue
        prefix,tail=line.split(': ',1)
        try:sc=parse_cost(prefix)
        except ValueError:continue
        if (re.fullmatch(r'(?:Target creature|This creature) gets [+-]\d+/[+-]\d+ until end of turn\.',tail)
                or tail.startswith('Create a ') and 'target' not in tail and 'Activate only' not in tail):siblings.append(sc)
        vector=mana(sc['mana'])
        if (sc['tap']==c['tap'] and not sc['sacrifice'] and not sc['discard'] and not sc['pay_life'] and not sc['x']
                and sum(vector)==1 and vector[0]==0 and not _can_pay(vector,mana(c['mana']))):alternative=next(j for j,v in enumerate(vector) if v)
    return dict(family=FAMILY,ability=ability,cost=c,power=int(m[2]),toughness=int(m[3]),scope='self' if m[1]=='This creature' else 'target',
                creature=creature,artifact='Artifact' in record['type_line'],activation_mana=mana(c['mana']),
                double_strike='Double strike' in record.get('keywords',[]),other_color=alternative,pt_siblings=siblings,other_clauses=[s for i,s in enumerate(lines) if i!=ability])


def _can_pay(have,need):
    return all(have[c]>=need[c] for c in range(1,6)) and sum(have)>=sum(need)


def variants(p):
    modes=['value','payment','offturn','ready_snapshot','pending_snapshot','resolved_snapshot','cleanup','counter_cleanup','source_removed_pending']
    if p['creature']:modes+=['fresh']
    if p['creature'] or p['artifact']:modes+=['tapped']
    if p['scope']=='target':modes+=['target_enemy','target_removed_pending','probe_noncreature']
    if p['scope']=='target' and p['creature']:modes+=['target_self']
    if sum(p['activation_mana']):modes+=['insufficient_mana']
    if any(p['activation_mana'][1:]):modes+=['wrong_color']
    if p['cost']['discard']:modes+=['empty_hand']
    if p['cost']['sacrifice']=='creature':modes+=['sacrifice_source']
    if p['cost']['sacrifice']!='self':modes+=['repeat']
    if p['cost']['tap'] and p['cost']['sacrifice']!='self':modes+=['repeat_untap']
    if p['scope']=='self' and p['toughness']<0 and not p['cost']['tap'] and not p['cost']['sacrifice']:modes+=['lethal_repeat']
    if p['double_strike']:modes+=['combat_zero','combat_positive']
    if p['other_color']!=-1:modes+=['other_mode_only']
    return [v+'_'+str(who) for who in (0,1) for v in modes]


def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2:raise ValueError('temporary PT family/integer version2 required')
    q=d.get('params')
    if not isinstance(q,dict) or set(q)!={'ability','interface','other_clauses'} or d.get('oracle_text')!=(r or {}).get('oracle_text'):raise ValueError('whole Oracle and selected clause/input contract required')
    p=parse(r,q['ability']);s=pins()['sources'].get(r['name'],{})
    expected=s.get('abilities',{}).get(str(p['ability']))
    controller_interfaces=s.get('controller_interfaces',{}).get(str(p['ability']),{})
    owners={v.rsplit('_',1)[-1] for v in d.get('variants',[]) if isinstance(v,str)}
    if owners & controller_interfaces.keys():
        if len(owners)!=1:raise ValueError('measured controller input requires a controller-specific declaration')
        expected=controller_interfaces[next(iter(owners))]
    variant_interfaces=s.get('variant_interfaces',{}).get(str(p['ability']),{})
    declared=d.get('variants',[])
    if any(isinstance(v,str) and v in variant_interfaces for v in declared):
        interfaces=[variant_interfaces.get(v,expected) if isinstance(v,str) else expected for v in declared]
        if any(v!=interfaces[0] for v in interfaces):raise ValueError('measured variant input requires a matching declaration')
        expected=interfaces[0]
    if (not s or any(r.get(k)!=v for k,v in s['oracle'].items()) or expected!=q['interface']
            or q['other_clauses']!=p['other_clauses']):raise ValueError('independent Oracle/interface changed')
    ui=q['interface']
    required={'mode_label','sacrifice_choices','discard_choices','sacrifice_before_target','other_color'}
    if not required<=set(ui) or set(ui)-required-{'menu_rule','fallback_color','zero_power_menu'}:raise ValueError('complete input protocol required')
    if ui['mode_label'] is not None and (not isinstance(ui['mode_label'],str) or not ui['mode_label'] or len(ui['mode_label'])>127 or '\n' in ui['mode_label']):raise ValueError('one real menu label required')
    if (type(ui['sacrifice_before_target']) is not bool or not isinstance(ui['sacrifice_choices'],dict)
            or not isinstance(ui['discard_choices'],dict) or type(ui['other_color']) is not int or not -1<=ui['other_color']<=5):raise ValueError('typed cost protocol required')
    if ui['other_color']!=p['other_color']:raise ValueError('sibling color input must match complete printed sibling cost')
    rule=ui.get('menu_rule','always')
    if rule not in ('always','grave_two_or_ai','power_less'):raise ValueError('unreviewed menu condition')
    if rule=='grave_two_or_ai' and not (p['scope']=='self' and p['cost']['discard']==1 and any(s.startswith('Exile two cards from your graveyard: ') for s in p['other_clauses'])):raise ValueError('grave menu rule requires printed two-card sibling')
    if rule=='power_less' and not any('Destroy target creature with power less than' in s for s in p['other_clauses']):raise ValueError('power menu requires printed power-bound sibling')
    if 'zero_power_menu' in ui and (type(ui['zero_power_menu']) is not bool or rule!='power_less'):raise ValueError('zero-power presentation annotation requires reviewed power menu')
    if type(ui.get('fallback_color',0)) is not int or not 0<=ui.get('fallback_color',0)<=6:raise ValueError('legal fallback color input required')
    if s.get('casting_colors'):
        from defensive_creature_recipes import reviewed_payment
        reviewed_payment(r,{'payment_colors':s['casting_colors']})
    for field in ('sacrifice_choices','discard_choices'):
        if any(k not in ('0','1') or type(v) is not bool for k,v in ui[field].items()):raise ValueError('controller input annotations must be booleans')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or any(not isinstance(v,str) for v in vs) or len(set(vs))!=len(vs) or not set(vs)<=set(variants(p)):raise ValueError('explicit distinct supported variants required')
    return p


def validate_witnesses(oracle,registry):
    for nm,pin in pins()['witnesses'].items():
        if registry.get(nm,{}).get('csvid')!=pin['csvid'] or any(oracle.get(nm,{}).get(k)!=v for k,v in pin['oracle'].items()):raise ValueError('temporary witness changed: '+nm)


def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step
    row=registry.get(name,{})
    if (row.get('kind')!='card' or row.get('csvid')!=csvid or pins()['sources'][name]['csvid']!=csvid
            or row.get('handler') in ('card_dummy','NULL','0',None)):
        raise ValueError('implemented primary identity required')
    validate_witnesses(oracle,registry)
    ui=d['params']['interface'];cost=p['cost'];out=[]
    for variant in selected:
        mode,pl=variant.rsplit('_',1);owner=int(pl);other=1-owner;setup=[];entities={};actions=[]
        zones={z:[[],[]] for z in ('hand','graveyard','library','exile')};life=[20,20];pools=[[30,10,10,10,10,10] for _ in (0,1)]

        def add(nm,key,who,zone='battlefield',resource=False):
            if nm!=name and nm not in pins()['witnesses']:raise ValueError('unpinned temporary entity')
            c=card(nm,key,who,zone)
            if zone=='battlefield':c['state']=['untapped']
            setup.append(c);entities[key]=dict(name=nm,player=who,alive=zone=='battlefield',tapped=False,sick=False,counter=0,power=0,toughness=0,damage=0,resource=resource)
            if zone!='battlefield':zones[zone][who].append(nm)

        def is_creature(key):return 'Creature' in oracle[entities[key]['name']]['type_line']
        def stat(key,k):return int(oracle[entities[key]['name']][k])+entities[key]['counter']+entities[key][k]
        def leave(key):entities[key]['alive']=False;zones['graveyard'][entities[key]['player']].append(entities[key]['name'])
        def fund(who,vector):return [step('set_mana',player=who,color=c,value=n) for c,n in enumerate(vector)]
        def check(stack=0):
            s=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for key,e in entities.items():
                if not e['alive'] or e['resource']:continue
                s += [step('require_card',ref=key,property='in_play',value=1),step('assert_card',ref=key,property='tapped',value=int(e['tapped'])),step('assert_counter',ref=key,counter_kind='plus1',value=e['counter'])]
                if is_creature(key):s += [step('require_card',ref=key,property=k,value=stat(key,k)) for k in ('power','toughness')]+[step('assert_card',ref=key,property='damage',value=e['damage'])]
                if key=='source' and p['creature']:s.append(step('assert_card',ref=key,property='sick',value=int(e['sick'])))
            names={e['name'] for e in entities.values()}|{'Plains','Forest','Island'}
            for who in (0,1):
                s += [step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=sum(pools[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                s += [step('assert_zone_multiplicity',player=who,zone='battlefield',index=registry[n]['csvid'],value=sum(e['alive'] and e['player']==who and e['name']==n for e in entities.values())) for n in sorted(names)]
                for z in zones:
                    ns=zones[z][who];s.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                    if z=='library':s += [step('assert_zone_card',player=who,zone=z,index=i,value=registry[n]['csvid']) for i,n in enumerate(ns)]
                    else:
                        counts=Counter(ns);s += [step('assert_zone_multiplicity',player=who,zone=z,index=registry[n]['csvid'],value=counts[n]) for n in sorted(names)]
            return s

        copies=(max(1,(int(oracle[name]['toughness'])-1)//(-p['toughness'])+1) if mode=='lethal_repeat'
                else 2 if mode in ('repeat','repeat_untap') else 1)
        if cost['sacrifice'] in ('creature','artifact','land') and mode!='sacrifice_source':
            for i in range(copies):add('Plains' if cost['sacrifice']=='land' else 'Ornithopter','victim'+str(i),owner,resource=True)
        if mode!='sacrifice_source':add('Bottle Gnomes','ally',owner)
        add('Bottle Gnomes','enemy',other);add('Sage of Lat-Nam','fragile',other);add('Icy Manipulator','invalid',owner)
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        if mode=='repeat_untap':add('Twiddle','untap',other,'hand')
        if mode=='counter_cleanup':add('Hunger of the Howlpack','counter_spell',owner,'hand')
        for who in (0,1):
            for z,nm in (('graveyard','Forest'),('library','Plains'),('exile','Island')):setup.append(card(nm,f'{z}_{who}',who,z));zones[z][who].append(nm)
            for i in range(copies if who==owner and cost['discard'] else 1):
                if who==owner and mode=='empty_hand':continue
                setup.append(card('Plains',f'hand_{who}_{i}',who,'hand'));zones['hand'][who].append('Plains')
        add(name,'source',owner,'hand')
        target=('source' if p['scope']=='self' or mode=='target_self' else 'enemy'
                if mode in ('target_enemy','sacrifice_source') or mode=='repeat' and p['toughness']<0 else 'ally')
        actions=[step('reset_death_history'),step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]
        if pins()['sources'][name].get('casting_colors'):
            from defensive_creature_recipes import reviewed_payment
            vector=reviewed_payment(oracle[name],{'payment_colors':pins()['sources'][name]['casting_colors']})[0]
        else:vector=mana(oracle[name]['mana_cost'])
        actions+=fund(owner,vector)+[step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=0)]
        zones['hand'][owner].remove(name);entities['source']['alive']=True;entities['source']['sick']=p['creature']
        if p['creature'] and mode!='fresh':actions += [step('advance_turn'),step('advance_turn')];entities['source']['sick']=False
        if mode=='counter_cleanup':
            actions += fund(owner,[0,0,0,1,0,0])+[step('choose_target',player=owner,ref=target),step('cast',ref='counter_spell',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=0)]
            zones['hand'][owner].remove('Hunger of the Howlpack');zones['graveyard'][owner].append('Hunger of the Howlpack');entities[target]['counter']=1
        if mode=='payment':pools[owner]=list(p['activation_mana'])
        if mode in ('insufficient_mana','empty_hand'):pools[owner]=[0]*6 if mode=='insufficient_mana' else pools[owner]
        if mode=='wrong_color':pools[owner]=[sum(p['activation_mana']),0,0,0,0,0]
        if mode=='other_mode_only':pools[owner]=[int(c==p['other_color']) for c in range(6)]
        actions+=fund(0,pools[0])+fund(1,pools[1])
        if mode=='offturn':actions.append(step('turn',player=other,value=0))
        if mode=='tapped':
            actions += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pools[other][0]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        actions += [step('refresh')];ready=len(actions)
        legal=mode not in ('insufficient_mana','wrong_color','empty_hand') and not(cost['tap'] and mode in ('fresh','tapped'))
        spent=0

        def menu_present():
            rule=ui.get('menu_rule','always')
            if rule=='grave_two_or_ai':return owner==1 or len(zones['graveyard'][owner])>=2
            if rule=='power_less':
                return (not entities['source']['tapped'] and not entities['source']['sick']
                        and (ui.get('zero_power_menu',False) and stat('source','power')==0
                             or any(e['alive'] and is_creature(k) and stat(k,'power')<stat('source','power') for k,e in entities.items())))
            return True

        def other_pt_legal():
            for sc in p['pt_siblings']:
                if not _can_pay(pools[owner],mana(sc['mana'])):continue
                if sc['tap'] and (entities['source']['tapped'] or entities['source']['sick']):continue
                if sc['discard'] and len(zones['hand'][owner])<sc['discard']:continue
                if sc['sacrifice'] not in (None,'self'):continue
                if sc['sacrifice']=='self' and not entities['source']['alive']:continue
                return True
            return False

        def activate():
            nonlocal spent
            s=[step('require_can_activate',ref='source',value=1)]
            with_menu=bool(ui['mode_label']) and menu_present()
            if with_menu:s.append(step('choose_activation_mode',ref='source',player=owner,mode_label=ui['mode_label'],fallback_color=ui.get('fallback_color',0),allow_auto_nonmana=False))
            victim='source' if cost['sacrifice']=='self' or mode=='sacrifice_source' else 'victim'+str(spent)
            sac=[step('choose_target',player=owner,ref=victim)] if cost['sacrifice'] not in (None,'self') and ui['sacrifice_choices'].get(str(owner),False) else []
            ts=[step('choose_target',player=owner,ref=target)] if p['scope']=='target' else []
            s += sac+ts if ui['sacrifice_before_target'] else ts+sac
            if cost['discard'] and ui['discard_choices'].get(str(owner),False):s.append(step('choose_zone_card',player=owner,value=registry['Plains']['csvid']))
            s.append(step('activate',ref='source',value=1))
            if with_menu:s.append(step('require_activation_mode',value=1))
            pools[owner]=[a-b for a,b in zip(pools[owner],p['activation_mana'])]
            if cost['tap']:entities['source']['tapped']=True
            if cost['sacrifice']:leave(victim)
            if cost['discard']:zones['hand'][owner].remove('Plains');zones['graveyard'][owner].append('Plains')
            spent+=1;return s+check(1)

        def resolve_effect():
            if entities[target]['alive']:
                entities[target]['power']+=p['power'];entities[target]['toughness']+=p['toughness']
                if stat(target,'toughness')<=0:leave(target)
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]

        if mode=='other_mode_only':actions.append(step('require_can_activate',ref='source',value=1))
        elif mode=='probe_noncreature':actions.append(step('probe_activation_target',ref='source',other='invalid',index=-1,value=0))
        elif not legal:
            if other_pt_legal():actions.append(step('require_can_activate',ref='source',value=1))
            else:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
        else:
            actions+=activate();pending=len(actions)
            if mode=='source_removed_pending' and entities['source']['alive']:actions += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='target_removed_pending':actions += [step('destroy',ref=target),step('refresh')];leave(target)
            actions+=resolve_effect()
            if mode in ('repeat','repeat_untap','lethal_repeat','combat_positive'):
                if cost['tap'] and mode=='repeat':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
                else:
                    if mode=='repeat_untap':
                        actions += [step('choose_target',player=other,ref='source'),step('cast',ref='untap',value=1),step('choose_dialog',player=other,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
                        zones['hand'][other].remove('Twiddle');zones['graveyard'][other].append('Twiddle');pools[other][2]-=1;entities['source']['tapped']=False
                    more=max(1,(int(oracle[name]['toughness'])-1)//(-p['toughness'])) if mode=='lethal_repeat' else 1
                    for i in range(more):actions+=activate()+resolve_effect()
            if mode in ('combat_zero','combat_positive'):
                actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
                entities['source']['tapped']=True;life[other]-=max(0,stat('source','power'))*(2 if p['double_strike'] else 1)
            if mode in ('cleanup','counter_cleanup'):
                actions += [step('cleanup'),step('refresh')]
                for e in entities.values():e['power']=e['toughness']=e['damage']=0
        final=check();actions+=final
        if mode=='payment' and legal:actions += [step('assert_mana',player=owner,color=c,value=0) for c in range(6)]
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending;tail=actions[cut:]
            actions=actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot' and legal:actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('set_life',player=other,value=9),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        out.append(dict(name=f'ActivatedTemporaryPTRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                        _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=[s for s in p['other_clauses'] if s!='Double strike']+['counter replacements','control changes','unselected sibling effects'])))
    return out
