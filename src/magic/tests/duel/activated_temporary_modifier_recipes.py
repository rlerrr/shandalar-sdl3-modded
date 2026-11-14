"""Pinned temporary keyword/PT clauses, real costs and behavioral receipts."""
import copy
import json
import re
from collections import Counter
from pathlib import Path
from permanent_value_activation_recipes import mana,parse_cost

FAMILY='activated_temporary_modifier'
DATA=Path(__file__).parent/'data/activated_temporary_modifier_wave8_profiles.json'
GRANTS=('Flying','First strike','Double strike','Deathtouch','Haste','Vigilance','Trample','Reach','Lifelink','Menace','Shroud','Hexproof','Indestructible')
REMINDERS={"(It can't be blocked except by creatures with flying or reach.)",
 '(It can deal excess combat damage to the player or planeswalker it\'s attacking.)',
 '(Any amount of damage it deals to a creature is enough to destroy it.)',
 '(Damage dealt by the creature also causes its controller to gain that much life.)',
 "(It can't be the target of spells or abilities.)"}

def pins():return json.loads(DATA.read_text(encoding='utf8'))

def parse(record,ability):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or any(k in record.get('type_line','') for k in ('Token ','Aura','Equipment','Planeswalker'))):raise ValueError('whole normal standalone permanent required')
    lines=record['oracle_text'].splitlines()
    if type(ability) is not int or not 0<=ability<len(lines) or ': ' not in lines[ability]:raise ValueError('complete selected activation required')
    prefix,effect=lines[ability].split(': ',1)
    for name in sorted({record['name'],record['name'].split(',')[0]},key=len,reverse=True):effect=effect.replace(name,'This creature')
    once=effect.endswith(' Activate only once each turn.')
    if once:effect=effect[:-len(' Activate only once each turn.')]
    reminder=re.search(r' (\([^\n]*\))$',effect)
    if reminder:
        if reminder[1] not in REMINDERS:raise ValueError('unreviewed keyword reminder')
        effect=effect[:reminder.start()]
    m=re.fullmatch(r'(This creature|Target creature|Target creature you control) (?:(?:gets ([+-]\d+)/([+-]\d+) and gains)|gains) (.*?) until end of turn\.',effect)
    if not m:raise ValueError('complete fixed temporary modifier clause required')
    canonical={k.lower():k for k in GRANTS};ks=re.split(r', and | and |, ',m[4])
    if not ks or any(k not in canonical for k in ks) or len(set(ks))!=len(ks):raise ValueError('explicit distinct fixed keywords required')
    if any(canonical[k] not in {'Flying','Reach','Deathtouch','First strike','Haste','Trample','Shroud','Double strike','Vigilance','Lifelink'} for k in ks):raise ValueError('keyword receipt branch awaits a delta pilot')
    c=parse_cost(prefix)
    # Only the four independently reviewed sacrifice forms enter the delta.
    if c['x'] or c['sacrifice'] not in (None,'land','artifact','creature','self') or 'counter' in c or 'loyalty' in c or c['discard'] not in (0,1):raise ValueError('unpiloted modifier cost')
    creature='Creature' in record['type_line'];land='Land' in record['type_line']
    if creature and (any(not re.fullmatch(r'-?\d+',record.get(k,'')) for k in ('power','toughness')) or int(record['toughness'])<=0):raise ValueError('numeric positive-toughness source required')
    if m[1]=='This creature' and not creature:raise ValueError('self effect needs creature source')
    others=[s for i,s in enumerate(lines) if i!=ability]
    if any(': ' not in s and not re.fullmatch(r'\(\{[WUBRG]/[WUBRG]\} can be paid with either \{[WUBRG]\} or \{[WUBRG]\}\.\)',s) for s in others):raise ValueError('unpiloted mandatory native source clause')
    siblings=[]
    for i,line in enumerate(lines):
        if i==ability or ': ' not in line:continue
        raw,tail=line.split(': ',1)
        try:sc=parse_cost(raw)
        except ValueError:continue
        if not sc['x'] and not sc['sacrifice'] and not sc['discard'] and not sc['pay_life'] and ('gains ' in tail and tail.endswith('until end of turn.') or tail=='Add {C}.'):siblings.append(dict(cost=sc,mana=tail=='Add {C}.'))
    return dict(family=FAMILY,ability=ability,cost=c,activation_mana=mana(c['mana']),power=int(m[2]) if m[2] is not None else 0,toughness=int(m[3]) if m[3] is not None else 0,grants=[canonical[k] for k in ks],scope='self' if m[1]=='This creature' else 'own' if m[1]=='Target creature you control' else 'target',once=once,creature=creature,land=land,other_clauses=others,siblings=siblings)

def variants(p):
    if _is_delta(p):return _delta_variants(p)
    v=['effect','payment','inactive','cleanup','counter_cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','source_removed_pending','repeat']
    if p['creature']:v+=['fresh','tapped']
    elif p['cost']['tap']:v+=['tapped']
    if p['scope']!='self':v+=['target_enemy','target_removed_pending','probe_noncreature']
    if p['scope']!='self' and p['creature']:v+=['target_self']
    if sum(p['activation_mana']):v+=['insufficient_mana']
    if any(p['activation_mana'][1:]):v+=['wrong_color']
    if p['cost']['discard']:v+=['empty_hand']
    if p['cost']['pay_life']:v+=['low_life','insufficient_life','minimum_life_query']
    if p['cost']['tap']:v+=['repeat_untap']
    if p['toughness']<0 and p['cost']['tap']:v+=['zero_toughness']
    if p['once']:v+=['once_reset']
    if set(p['grants'])&{'Flying','Reach'}:v+=['ground_block']
    if any(s['mana'] for s in p['siblings']):v+=['mana_sibling']
    elif any(s['cost']['mana']!=p['cost']['mana'] for s in p['siblings']):v+=['other_mode_only']
    if len(p['grants'])>1:v+=['keyword_'+g.lower().replace(' ','_') for g in p['grants']]
    return [s+'_'+str(who) for who in (0,1) for s in v]

def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2:raise ValueError('modifier family/integer version2 required')
    q=d.get('params')
    if not isinstance(q,dict) or set(q)!={'ability','interface','other_clauses'} or d.get('oracle_text')!=(r or {}).get('oracle_text'):raise ValueError('whole Oracle/selected clause and interface required')
    p=parse(r,q['ability']);s=pins()['sources'].get(r['name'],{})
    if (not s or any(r.get(k)!=v for k,v in s['oracle'].items()) or s['abilities'].get(str(p['ability']))!=q['interface'] or q['other_clauses']!=p['other_clauses']):raise ValueError('independent Oracle/interface changed')
    if 'measured_cost_victims' in s:_validate_measured_cost_victims(p,s)
    ui=q['interface']
    fields={'mode_label','fallback_color','allow_auto_nonmana','discard_choices','probe_dialog'}
    if _is_delta(p):fields|={'sacrifice_choices','sacrifice_before_target'}
    if not isinstance(ui,dict) or set(ui)!=fields:raise ValueError('complete typed actual input interface required')
    if _is_delta(p) and (type(ui['sacrifice_before_target']) is not bool or not isinstance(ui['sacrifice_choices'],dict) or set(ui['sacrifice_choices'])!={'0','1'} or any(type(v) is not bool for v in ui['sacrifice_choices'].values())):raise ValueError('typed actual cost-choice interface required')
    label=ui['mode_label']
    if label is not None and (not isinstance(label,str) or not label or len(label)>127 or '\n' in label):raise ValueError('actual one-line mode label required')
    if type(ui['fallback_color']) is not int or not 0<=ui['fallback_color']<=6 or type(ui['allow_auto_nonmana']) is not bool:raise ValueError('typed legal mode input required')
    if ui['probe_dialog'] is not None and (type(ui['probe_dialog']) is not int or not 0<=ui['probe_dialog']<=8):raise ValueError('reviewed bounded probe dialog input required')
    if ui['allow_auto_nonmana'] and not (p['land'] and len(p['siblings'])==1 and p['siblings'][0]['mana']):raise ValueError('auto only for reviewed unique nonmana land sibling')
    if not isinstance(ui['discard_choices'],dict) or any(k not in ('0','1') or type(v) is not bool for k,v in ui['discard_choices'].items()):raise ValueError('actual controller discard inputs required')
    if s.get('casting_colors'):
        from defensive_creature_recipes import reviewed_payment
        reviewed_payment(r,{'payment_colors':s['casting_colors']})
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or any(not isinstance(v,str) for v in vs) or len(set(vs))!=len(vs) or not set(vs)<=set(variants(p)):raise ValueError('explicit distinct supported variants required')
    return p

def validate_witnesses(oracle,registry):
    for n,s in pins()['witnesses'].items():
        row=registry.get(n,{})
        if row.get('csvid')!=s['csvid'] or row.get('kind')!='card' or row.get('handler')!=s['handler'] or row.get('handler') in ('card_dummy',None,'NULL','0') or any(oracle.get(n,{}).get(k)!=v for k,v in s['oracle'].items()):raise ValueError('implemented independent modifier witness changed: '+n)

def _payable(have,need):return all(have[c]>=need[c] for c in range(1,6)) and sum(have)>=sum(need)

def expand(name,csvid,d,p,selected,registry,oracle):
    if _is_delta(p):return _expand_delta(name,csvid,d,p,selected,registry,oracle)
    from recipes import card,step
    from defensive_creature_recipes import reviewed_payment
    row=registry.get(name,{})
    if row.get('csvid')!=csvid or row.get('kind')!='card' or row.get('handler') in ('card_dummy',None,'NULL','0') or pins()['sources'][name]['csvid']!=csvid or pins()['sources'][name]['handler']!=row.get('handler'):raise ValueError('implemented primary required')
    validate_witnesses(oracle,registry);ui=d['params']['interface'];cost=p['cost'];results=[]
    for variant in selected:
        mode,pl=variant.rsplit('_',1);owner=int(pl);other=1-owner;setup=[];entities={};steps=[]
        zones={z:[[],[]] for z in ('hand','graveyard','library','exile')};life=[20,20];pools=[[30,10,10,10,10,10] for _ in (0,1)];used=0
        probe_key=mode.removeprefix('keyword_').replace('_',' ').title() if mode.startswith('keyword_') else p['grants'][0]
        if probe_key=='First Strike':probe_key='First strike'
        defensive=probe_key in ('Flying','Reach','First strike','Deathtouch') and mode!='ground_block'
        target='source' if p['scope']=='self' or mode=='target_self' else 'enemy' if mode=='target_enemy' else 'ally'
        target_owner=owner if target in ('source','ally') else other
        combat_turn=1-target_owner if defensive else target_owner

        def add(nm,key,who,zone='battlefield'):
            if nm!=name and nm not in pins()['witnesses']:raise ValueError('unpinned modifier fixture')
            c=card(nm,key,who,zone)
            if zone=='battlefield':c['state']=['untapped']
            setup.append(c);entities[key]=dict(name=nm,player=who,alive=zone=='battlefield',tapped=False,sick=False,counter=0,power=0,toughness=0,damage=0,keys={'Flying'} if nm=="Judge's Familiar" else set())
            if zone!='battlefield':zones[zone][who].append(nm)
        def creature(key):return 'Creature' in oracle[entities[key]['name']]['type_line']
        def stat(key,k):e=entities[key];return int(oracle[e['name']][k])+e['counter']+e[k]
        def leave(key,zone='graveyard'):e=entities[key];e['alive']=False;zones[zone][e['player']].append(e['name'])
        def fund(who,vector):return [step('set_mana',player=who,color=c,value=n) for c,n in enumerate(vector)]
        def check(stack=0):
            s=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for key,e in entities.items():
                if not e['alive']:continue
                s += [step('require_card',ref=key,property='in_play',value=1),step('assert_card',ref=key,property='tapped',value=int(e['tapped'])),step('assert_counter',ref=key,counter_kind='plus1',value=e['counter'])]
                # e.sick retains control age for expiry/attack rules. The API
                # exposes effective state bits, cleared while Haste is active
                # and restored from host control-age state when it expires.
                if creature(key):s += [step('require_card',ref=key,property=k,value=stat(key,k)) for k in ('power','toughness')]+[step('assert_card',ref=key,property='damage',value=e['damage']),step('assert_card',ref=key,property='sick',value=int(e['sick'] and 'Haste' not in e['keys']))]
            names={e['name'] for e in entities.values()}|{'Plains','Forest','Island'}
            for who in (0,1):
                s += [step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=sum(pools[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                if who==owner:
                    if not p['activation_mana'][0]:s += [step('assert_mana',player=who,color=c,value=n) for c,n in enumerate(pools[who])]
                    else:s += [step('assert_mana_at_most',player=who,color=c,value=pools[who][c]) for c in range(1,6) if p['activation_mana'][c]]
                s += [step('assert_zone_multiplicity',player=who,zone='battlefield',index=registry[n]['csvid'],value=sum(e['alive'] and e['player']==who and e['name']==n for e in entities.values())) for n in sorted(names)]
                for z in zones:
                    ns=zones[z][who];s.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                    if z=='library':s += [step('assert_zone_card',player=who,zone=z,index=i,value=registry[n]['csvid']) for i,n in enumerate(ns)]
                    else:
                        counts=Counter(ns);s += [step('assert_zone_multiplicity',player=who,zone=z,index=registry[n]['csvid'],value=counts[n]) for n in sorted(names)]
            return s

        fresh_target='Haste' in p['grants'] and target!='source'
        for key,who in (('ally',owner),('enemy',other)):
            add('Sage of Lat-Nam' if mode=='zero_toughness' and key==target else 'Scion of Glaciers',key,who,'hand' if fresh_target and key==target else 'battlefield')
        add('Icy Manipulator','invalid',owner)
        blocker_name='Bottle Gnomes' if mode=='ground_block' else 'Elvish Mystic' if probe_key=='First strike' and p['scope']=='self' and int(oracle[name]['power'])<2 else "Judge's Familiar" if probe_key in ('Flying','Reach','Trample') or 'Flying' in p['grants'] else 'Scion of Glaciers' if probe_key=='Deathtouch' else 'Sage of Lat-Nam'
        add(blocker_name,'combat_witness',combat_turn if defensive else 1-target_owner)
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        if mode in ('repeat_untap','zero_toughness') or mode=='tapped' and p['scope']=='self' and not cost['tap']:add('Twiddle','untap',other,'hand')
        if mode=='counter_cleanup':add('Hunger of the Howlpack','counter_spell',owner,'hand')
        for who in (0,1):
            for z,nm in (('graveyard','Forest'),('library','Plains'),('exile','Island')):setup.append(card(nm,f'{z}_{who}',who,z));zones[z][who].append(nm)
            for i in range(2 if cost['discard'] and mode in ('repeat','repeat_untap','zero_toughness') and who==owner else 1):
                if who==owner and mode=='empty_hand':continue
                setup.append(card('Plains',f'hand_{who}_{i}',who,'hand'));zones['hand'][who].append('Plains')
        add(name,'source',owner,'battlefield' if p['land'] else 'hand')
        steps=[step('reset_death_history'),step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]

        def cast(key):
            e=entities[key];who=e['player'];nm=e['name']
            colors=pins()['sources'][name].get('casting_colors') if key=='source' else None
            vector=reviewed_payment(oracle[nm],{'payment_colors':colors})[0] if colors else mana(oracle[nm]['mana_cost'])
            seq=fund(who,vector)+[step('cast',ref=key,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=who,value=0)]
            zones['hand'][who].remove(nm);e['alive']=True;e['sick']=creature(key)
            return seq
        if not p['land']:
            steps+=cast('source')
            if p['creature'] and mode!='fresh':steps += [step('advance_turn'),step('advance_turn')];entities['source']['sick']=False
        if fresh_target:
            steps += [step('turn',player=target_owner,value=0)]+cast(target)
        if mode=='counter_cleanup':
            steps += fund(owner,[0,0,0,1,0,0])+[step('choose_target',player=owner,ref=target),step('cast',ref='counter_spell',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=0)]
            zones['hand'][owner].remove('Hunger of the Howlpack');zones['graveyard'][owner].append('Hunger of the Howlpack');entities[target]['counter']=1
        if mode=='payment':pools[owner]=list(p['activation_mana'])
        if mode in ('insufficient_mana','mana_sibling'):pools[owner]=[0]*6
        if mode=='wrong_color':pools[owner]=[sum(p['activation_mana']),0,0,0,0,0]
        if mode=='other_mode_only':pools[owner]=mana(next(s['cost']['mana'] for s in p['siblings'] if not s['mana'] and s['cost']['mana']!=cost['mana']))
        if mode in ('low_life','insufficient_life','minimum_life_query'):
            life[owner]=cost['pay_life']+1 if mode=='low_life' else cost['pay_life']-1 if mode=='insufficient_life' else cost['pay_life'];steps.append(step('set_life',player=owner,value=life[owner]))
        steps += fund(0,pools[0])+fund(1,pools[1])+[step('turn',player=combat_turn,value=0)]
        if mode=='tapped':
            steps += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pools[other][0]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        steps.append(step('refresh'));ready=len(steps)

        def sibling_legal():
            return any(_payable(pools[owner],mana(s['cost']['mana'])) and not(s['cost']['tap'] and (entities['source']['tapped'] or entities['source']['sick'])) for s in p['siblings'])
        def deny():
            sibling=sibling_legal();return [step('require_can_activate',ref='source',value=int(sibling))]+([] if sibling else [step('activate',ref='source',value=0)])
        def activate():
            nonlocal used
            s=[step('require_can_activate',ref='source',value=1)]
            if ui['mode_label']:s.append(step('choose_activation_mode',ref='source',player=owner,mode_label=ui['mode_label'],fallback_color=ui['fallback_color'],allow_auto_nonmana=ui['allow_auto_nonmana']))
            if p['scope']!='self':s.append(step('choose_target',player=owner,ref=target))
            if cost['discard'] and ui['discard_choices'].get(str(owner),False):s.append(step('choose_zone_card',player=owner,value=registry['Plains']['csvid']))
            s.append(step('activate',ref='source',value=1))
            if ui['mode_label']:s.append(step('require_activation_mode',value=1))
            pools[owner]=[a-b for a,b in zip(pools[owner],p['activation_mana'])];life[owner]-=cost['pay_life']
            if cost['tap']:entities['source']['tapped']=True
            if cost['discard']:zones['hand'][owner].remove('Plains');zones['graveyard'][owner].append('Plains')
            used+=1;return s+check(1)
        def resolve():
            if entities[target]['alive']:
                e=entities[target];e['power']+=p['power'];e['toughness']+=p['toughness'];e['keys'].update(p['grants'])
                if stat(target,'toughness')<=0:leave(target)
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]
        def untap():
            seq=[step('choose_target',player=other,ref='source'),step('cast',ref='untap',value=1),step('choose_dialog',player=other,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            zones['hand'][other].remove('Twiddle');zones['graveyard'][other].append('Twiddle');pools[other][2]-=1;entities['source']['tapped']=False;return seq
        legal=mode not in ('insufficient_mana','wrong_color','empty_hand','insufficient_life') and not(cost['tap'] and mode in ('fresh','tapped'))
        pending=ready
        if mode=='mana_sibling':
            steps += [step('require_can_activate',ref='source',value=1),step('activate',ref='source',value=1),step('assert_stack',value=0),step('assert_choices_consumed')]
            pools[owner][0]+=1;entities['source']['tapped']=True;steps += [step('require_can_activate',ref='source',value=0)]
        elif mode=='other_mode_only' or mode=='minimum_life_query':steps.append(step('require_can_activate',ref='source',value=1))
        elif mode=='probe_noncreature':steps.append(step('probe_activation_target',ref='source',other='invalid',index=-1 if ui['probe_dialog'] is None else ui['probe_dialog'],value=0))
        elif not legal:steps+=deny()
        elif mode!='inactive':
            steps+=activate();pending=len(steps)
            if mode=='source_removed_pending':steps += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='target_removed_pending':steps += [step('destroy',ref=target),step('refresh')];leave(target)
            steps+=resolve()
            if mode=='repeat':
                if cost['tap'] or p['once']:steps+=deny()
                else:steps+=activate()+resolve()
            if mode in ('repeat_untap','zero_toughness'):steps+=untap()+activate()+resolve()
            if mode=='once_reset':
                steps+=deny()+[step('advance_turn'),step('advance_turn')]
                for e in entities.values():e['power']=e['toughness']=e['damage']=0;e['keys']={'Flying'} if e['name']=="Judge's Familiar" else set()
                steps += [step('turn',player=combat_turn,value=0)]+activate()+resolve()
            if mode=='tapped' and p['scope']=='self' and not cost['tap']:steps+=untap()
            if mode in ('cleanup','counter_cleanup'):
                steps += [step('cleanup'),step('refresh')]
                for e in entities.values():e['power']=e['toughness']=e['damage']=0;e['keys']={'Flying'} if e['name']=="Judge's Familiar" else set()

        # The keyword is proved by legal combat and its actual damage result,
        # never by an engine-keyword bit or by the mere existence of a menu.
        probe=legal and mode not in ('mana_sibling','other_mode_only','minimum_life_query','probe_noncreature') and entities[target]['alive']
        if probe:
            e=entities[target];w=entities['combat_witness'];keys=e['keys']
            steps += [step('turn',player=combat_turn,value=0),step('begin_attackers')]
            attacker='combat_witness' if defensive else target;blocker=target if defensive else 'combat_witness'
            can_attack=defensive or not e['tapped'] and (not e['sick'] or 'Haste' in keys)
            steps += [step('attack',ref=attacker,value=int(can_attack))]
            if can_attack:entities[attacker]['tapped']=True
            steps += [step('finish_attackers'),step('begin_blockers')]
            blocks=bool(can_attack and (not e['tapped'] and ('Flying' in keys or 'Reach' in keys or probe_key not in ('Flying','Reach')) if defensive else probe_key!='Haste' and ('Flying' not in keys or 'Flying' in w['keys'] or 'Reach' in w['keys'])))
            if can_attack and (defensive or probe_key!='Haste'):steps += [step('block',ref=blocker,other=attacker,value=int(blocks))]
            steps += [step('advance_combat'),step('combat_damage'),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            if can_attack:
                if not blocks:life[1-entities[attacker]['player']]-=max(0,stat(attacker,'power'))
                else:
                    a,b=attacker,blocker;ak=entities[a]['keys'];bk=entities[b]['keys'];ap=max(0,stat(a,'power'));bp=max(0,stat(b,'power'))
                    afirst='First strike' in ak; bfirst='First strike' in bk
                    kill_b=ap>0 and (ap>=stat(b,'toughness') or 'Deathtouch' in ak);kill_a=bp>0 and (bp>=stat(a,'toughness') or 'Deathtouch' in bk)
                    dealt_a=0 if bfirst and kill_a and not afirst else ap;dealt_b=0 if afirst and kill_b and not bfirst else bp
                    excess=max(0,dealt_a-(1 if 'Deathtouch' in ak else stat(b,'toughness'))) if 'Trample' in ak else 0
                    life[1-entities[a]['player']]-=excess
                    if dealt_a and (dealt_a>=stat(b,'toughness') or 'Deathtouch' in ak):leave(b)
                    else:entities[b]['damage']+=dealt_a
                    if dealt_b and (dealt_b>=stat(a,'toughness') or 'Deathtouch' in bk):leave(a)
                    else:entities[a]['damage']+=dealt_b
            steps += [step('end_combat'),step('assert_choices_consumed')]
        final=check();steps+=final
        if mode=='payment' and legal:steps += [step('assert_mana',player=owner,color=c,value=0) for c in range(6)]
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending;tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('set_life',player=other,value=9),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        gaps=list(p['other_clauses'])
        if mode=='mana_sibling':gaps=[s for s in gaps if s!='{T}: Add {C}.']
        if p['land']:gaps+=['land-play history (source starts as a normal battlefield land fixture)']
        results.append(dict(name=f'ActivatedTemporaryModifierRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps+['control changes','blink identity','additional recipient native keywords'])))
    return results


# Scoped held11 delta: the preceding accepted expansion remains unchanged.
def _is_delta(p):
    return bool(p['cost']['sacrifice'] or set(p['grants'])&{'Shroud','Double strike','Vigilance','Lifelink'})

def _delta_variants(p):
    v=['effect','payment','inactive','cleanup','counter_cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','source_removed_pending']
    if p['cost']['sacrifice']!='self':v+=['repeat']
    if p['creature']:v+=['fresh','tapped']
    elif p['cost']['tap']:v+=['tapped']
    if p['scope']!='self':
        v+=['probe_enemy' if p['scope']=='own' else 'target_enemy','target_removed_pending','probe_noncreature']
        if p['creature']:v+=['target_self']
    if sum(p['activation_mana']):v+=['insufficient_mana']
    if any(p['activation_mana'][1:]):v+=['wrong_color']
    if p['cost']['tap']:v+=['repeat_untap']
    if p['cost']['sacrifice'] in ('land','artifact'):v+=['no_resource']
    if p['cost']['sacrifice']=='creature':v+=['source_sacrifice','repeat_source']
    if set(p['grants'])&{'Double strike','Lifelink'}:v+=['blocked']
    if 'Flying' in p['grants']:v+=['ground_block']
    if any(s['mana'] for s in p['siblings']):v+=['mana_sibling']
    if len(p['grants'])>1:v+=['keyword_'+g.lower().replace(' ','_') for g in p['grants']]
    return [s+'_'+str(who) for who in (0,1) for s in v]

def _validate_measured_cost_victims(p,source):
    # A receipt of a measured automatic legal AI decision, not an Oracle
    # exclusion of other victims and not an unoffered decision-queue input.
    measured=source.get('measured_cost_victims')
    positive={'effect','payment','cleanup','counter_cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','source_removed_pending','repeat','fresh','tapped','ground_block','repeat_source'}
    if (not isinstance(measured,dict) or not measured or p['cost']['sacrifice']!='creature'
            or p['scope']!='self' or any(type(k) is not str or v!='source' or not k.endswith('_1') or k.rsplit('_',1)[0] not in positive or k not in _delta_variants(p) for k,v in measured.items())
            or source['abilities'][str(p['ability'])]['sacrifice_choices'].get('1') is not False):
        raise ValueError('measured automatic legal AI creature-cost victim required')


def _expand_delta(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step
    from defensive_creature_recipes import reviewed_payment
    row=registry.get(name,{})
    if row.get('csvid')!=csvid or row.get('kind')!='card' or row.get('handler') in ('card_dummy',None,'NULL','0') or pins()['sources'][name]['csvid']!=csvid or pins()['sources'][name]['handler']!=row.get('handler'):raise ValueError('implemented primary required')
    validate_witnesses(oracle,registry);ui=d['params']['interface'];cost=p['cost'];results=[]
    for variant in selected:
        mode,pl=variant.rsplit('_',1);owner=int(pl);other=1-owner;setup=[];entities={};steps=[]
        zones={z:[[],[]] for z in ('hand','graveyard','library','exile')};life=[20,20];pools=[[30,10,10,10,10,10] for _ in (0,1)];used=0
        probe_key=mode.removeprefix('keyword_').replace('_',' ').title() if mode.startswith('keyword_') else p['grants'][0]
        if probe_key=='First Strike':probe_key='First strike'
        if probe_key=='Double Strike':probe_key='Double strike'
        sac=cost['sacrifice'];resources=[];source_only=mode=='source_sacrifice'
        resource_name='Plains' if sac=='land' else 'Shield Sphere'
        measured_victim=pins()['sources'][name].get('measured_cost_victims',{}).get(variant)
        defensive=probe_key in ('Flying','Reach','First strike','Deathtouch') and mode not in ('ground_block','repeat_source')
        target='source' if p['scope']=='self' or mode=='target_self' else 'enemy' if mode=='target_enemy' else 'ally'
        target_owner=owner if target in ('source','ally') else other
        combat_turn=1-target_owner if defensive else target_owner

        def add(nm,key,who,zone='battlefield'):
            if nm!=name and nm not in pins()['witnesses']:raise ValueError('unpinned modifier fixture')
            c=card(nm,key,who,zone)
            if zone=='battlefield':c['state']=['untapped']
            setup.append(c);entities[key]=dict(name=nm,player=who,alive=zone=='battlefield',tapped=False,sick=False,counter=0,power=0,toughness=0,damage=0,keys={'Flying'} if nm=="Judge's Familiar" else set())
            if zone!='battlefield':zones[zone][who].append(nm)
        def creature(key):return 'Creature' in oracle[entities[key]['name']]['type_line']
        def stat(key,k):e=entities[key];return int(oracle[e['name']][k])+e['counter']+e[k]
        def leave(key,zone='graveyard'):e=entities[key];e['alive']=False;zones[zone][e['player']].append(e['name'])
        def fund(who,vector):return [step('set_mana',player=who,color=c,value=n) for c,n in enumerate(vector)]
        def check(stack=0):
            s=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for key,e in entities.items():
                # Identical cost resources have interchangeable legal AI slots.
                # Their exact battlefield/grave CSVID counts remain asserted.
                if not e['alive'] or key in resources:continue
                s += [step('require_card',ref=key,property='in_play',value=1),step('assert_card',ref=key,property='tapped',value=int(e['tapped'])),step('assert_counter',ref=key,counter_kind='plus1',value=e['counter'])]
                # e.sick retains control age for expiry/attack rules. The API
                # exposes effective state bits, cleared while Haste is active
                # and restored from host control-age state when it expires.
                if creature(key):s += [step('require_card',ref=key,property=k,value=stat(key,k)) for k in ('power','toughness')]+[step('assert_card',ref=key,property='damage',value=e['damage']),step('assert_card',ref=key,property='sick',value=int(e['sick'] and 'Haste' not in e['keys']))]
            names={e['name'] for e in entities.values()}|{'Plains','Forest','Island'}
            for who in (0,1):
                s += [step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=sum(pools[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                if who==owner:
                    if not p['activation_mana'][0]:s += [step('assert_mana',player=who,color=c,value=n) for c,n in enumerate(pools[who])]
                    else:s += [step('assert_mana_at_most',player=who,color=c,value=pools[who][c]) for c in range(1,6) if p['activation_mana'][c]]
                s += [step('assert_zone_multiplicity',player=who,zone='battlefield',index=registry[n]['csvid'],value=sum(e['alive'] and e['player']==who and e['name']==n for e in entities.values())) for n in sorted(names)]
                for z in zones:
                    ns=zones[z][who];s.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                    if z=='library':s += [step('assert_zone_card',player=who,zone=z,index=i,value=registry[n]['csvid']) for i,n in enumerate(ns)]
                    else:
                        counts=Counter(ns);s += [step('assert_zone_multiplicity',player=who,zone=z,index=registry[n]['csvid'],value=counts[n]) for n in sorted(names)]
            return s

        fresh_target='Haste' in p['grants'] and target!='source'
        for key,who in (('ally',owner),('enemy',other)):
            if key=='ally' and p['scope']=='self':continue
            add('Scion of Glaciers',key,who,'hand' if fresh_target and key==target else 'battlefield')
        add('Icy Manipulator','invalid',other)
        if sac in ('land','artifact','creature') and mode!='no_resource' and not source_only:
            for i in range(3 if 'Shroud' in p['grants'] else 2 if mode=='repeat' else 1):
                key=f'cost_{i}';add(resource_name,key,owner);resources.append(key)
        if 'Shroud' in p['grants']:
            for who in (0,1):add('Unsummon',f'probe_{who}',who,'hand')
        blocker_name='Bottle Gnomes' if mode in ('ground_block','repeat_source') else "Judge's Familiar" if probe_key in ('Flying','Reach','Trample') or 'Flying' in p['grants'] else 'Scion of Glaciers' if probe_key in ('Deathtouch','Double strike','Lifelink') else 'Sage of Lat-Nam'
        add(blocker_name,'combat_witness',combat_turn if defensive else 1-target_owner)
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        if mode in ('repeat_untap','zero_toughness') or mode=='tapped' and p['scope']=='self' and not cost['tap']:add('Twiddle','untap',other,'hand')
        if mode=='counter_cleanup':add('Hunger of the Howlpack','counter_spell',owner,'hand')
        for who in (0,1):
            for z,nm in (('graveyard','Forest'),('library','Plains'),('exile','Island')):setup.append(card(nm,f'{z}_{who}',who,z));zones[z][who].append(nm)
            for i in range(2 if cost['discard'] and mode in ('repeat','repeat_untap','zero_toughness') and who==owner else 1):
                if who==owner and mode=='empty_hand':continue
                setup.append(card('Plains',f'hand_{who}_{i}',who,'hand'));zones['hand'][who].append('Plains')
        add(name,'source',owner,'battlefield' if p['land'] else 'hand')
        steps=[step('reset_death_history'),step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]

        def cast(key):
            e=entities[key];who=e['player'];nm=e['name']
            colors=pins()['sources'][name].get('casting_colors') if key=='source' else None
            vector=reviewed_payment(oracle[nm],{'payment_colors':colors})[0] if colors else mana(oracle[nm]['mana_cost'])
            seq=fund(who,vector)+[step('cast',ref=key,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=who,value=0)]
            zones['hand'][who].remove(nm);e['alive']=True;e['sick']=creature(key)
            return seq
        if not p['land']:
            steps+=cast('source')
            if p['creature'] and mode!='fresh':steps += [step('advance_turn'),step('advance_turn')];entities['source']['sick']=False
        if fresh_target:
            steps += [step('turn',player=target_owner,value=0)]+cast(target)
        if mode=='counter_cleanup':
            steps += fund(owner,[0,0,0,1,0,0])+[step('choose_target',player=owner,ref=target),step('cast',ref='counter_spell',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=0)]
            zones['hand'][owner].remove('Hunger of the Howlpack');zones['graveyard'][owner].append('Hunger of the Howlpack');entities[target]['counter']=1
        if mode=='payment':pools[owner]=list(p['activation_mana'])
        if mode in ('insufficient_mana','mana_sibling'):pools[owner]=[0]*6
        if mode=='wrong_color':pools[owner]=[sum(p['activation_mana']),0,0,0,0,0]
        if mode=='other_mode_only':pools[owner]=mana(next(s['cost']['mana'] for s in p['siblings'] if not s['mana'] and s['cost']['mana']!=cost['mana']))
        if mode in ('low_life','insufficient_life','minimum_life_query'):
            life[owner]=cost['pay_life']+1 if mode=='low_life' else cost['pay_life']-1 if mode=='insufficient_life' else cost['pay_life'];steps.append(step('set_life',player=owner,value=life[owner]))
        steps += fund(0,pools[0])+fund(1,pools[1])+[step('turn',player=combat_turn,value=0)]
        if mode=='tapped':
            steps += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pools[other][0]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        steps.append(step('refresh'));ready=len(steps)

        def sibling_legal():
            # The global query includes every printed simple sibling, including
            # Brightling's PT/bounce and Vizkopa's non-targeted delayed effect.
            for i,line in enumerate(oracle[name]['oracle_text'].splitlines()):
                if i==p['ability'] or ': ' not in line:continue
                sc=parse_cost(line.split(': ',1)[0])
                if sc['sacrifice'] or sc['discard'] or sc['pay_life'] or sc['x']:continue
                if _payable(pools[owner],mana(sc['mana'])) and not(sc['tap'] and (entities['source']['tapped'] or entities['source']['sick'])):return True
            return False
        def deny():
            sibling=sibling_legal();return [step('require_can_activate',ref='source',value=int(sibling))]+([] if sibling else [step('activate',ref='source',value=0)])
        def activate(victim=None):
            nonlocal used
            if sac=='self':victim='source'
            elif sac in ('land','artifact','creature') and victim is None:
                victim=measured_victim or ('source' if source_only else next(k for k in resources if entities[k]['alive']))
            cost_choice=bool(victim and sac!='self' and ui['sacrifice_choices'][str(owner)])
            s=[step('require_can_activate',ref='source',value=1)]
            if ui['mode_label']:s.append(step('choose_activation_mode',ref='source',player=owner,mode_label=ui['mode_label'],fallback_color=ui['fallback_color'],allow_auto_nonmana=ui['allow_auto_nonmana']))
            if cost_choice and ui['sacrifice_before_target']:s.append(step('choose_target',player=owner,ref=victim))
            if p['scope']!='self':s.append(step('choose_target',player=owner,ref=target))
            if cost_choice and not ui['sacrifice_before_target']:s.append(step('choose_target',player=owner,ref=victim))
            if cost['discard'] and ui['discard_choices'].get(str(owner),False):s.append(step('choose_zone_card',player=owner,value=registry['Plains']['csvid']))
            s.append(step('activate',ref='source',value=1))
            if ui['mode_label']:s.append(step('require_activation_mode',value=1))
            pools[owner]=[a-b for a,b in zip(pools[owner],p['activation_mana'])];life[owner]-=cost['pay_life']
            if cost['tap']:entities['source']['tapped']=True
            if cost['discard']:zones['hand'][owner].remove('Plains');zones['graveyard'][owner].append('Plains')
            if victim:leave(victim)
            used+=1;return s+check(1)
        def resolve():
            if entities[target]['alive']:
                e=entities[target];e['power']+=p['power'];e['toughness']+=p['toughness'];e['keys'].update(p['grants'])
                if stat(target,'toughness')<=0:leave(target)
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]
        def untap():
            seq=[step('choose_target',player=other,ref='source'),step('cast',ref='untap',value=1),step('choose_dialog',player=other,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            zones['hand'][other].remove('Twiddle');zones['graveyard'][other].append('Twiddle');pools[other][2]-=1;entities['source']['tapped']=False;return seq
        legal=mode not in ('insufficient_mana','wrong_color','no_resource') and not(cost['tap'] and mode in ('fresh','tapped'))
        pending=ready
        if mode=='mana_sibling':
            steps += [step('require_can_activate',ref='source',value=1),step('activate',ref='source',value=1),step('assert_stack',value=0),step('assert_choices_consumed')]
            pools[owner][0]+=1;entities['source']['tapped']=True;steps += [step('require_can_activate',ref='source',value=0)]
        elif mode=='other_mode_only' or mode=='minimum_life_query':steps.append(step('require_can_activate',ref='source',value=1))
        elif mode in ('probe_noncreature','probe_enemy'):
            steps.append(step('probe_activation_target',ref='source',other='enemy' if mode=='probe_enemy' else 'invalid',index=-1 if ui['probe_dialog'] is None else ui['probe_dialog'],value=0))
        elif not legal:steps+=deny()
        elif mode!='inactive':
            steps+=activate();pending=len(steps)
            if mode=='source_removed_pending' and entities['source']['alive']:
                steps += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='target_removed_pending':steps += [step('destroy',ref=target),step('refresh')];leave(target)
            steps+=resolve()
            if mode=='repeat' and entities['source']['alive']:
                if cost['tap'] or p['once'] or sac=='self':
                    if entities['source']['alive']:steps+=deny()
                else:
                    # Shroud forbids selecting the same recipient again; a
                    # distinct own source is a legal second protected creature.
                    if 'Shroud' in p['grants']:target='source';target_owner=owner
                    steps+=activate()+resolve()
            if mode in ('repeat_untap','zero_toughness'):steps+=untap()+activate()+resolve()
            if mode=='once_reset':
                steps+=deny()+[step('advance_turn'),step('advance_turn')]
                for e in entities.values():e['power']=e['toughness']=e['damage']=0;e['keys']={'Flying'} if e['name']=="Judge's Familiar" else set()
                steps += [step('turn',player=combat_turn,value=0)]+activate()+resolve()
            if mode=='tapped' and p['scope']=='self' and not cost['tap'] and entities['source']['alive']:steps+=untap()
            if mode in ('cleanup','counter_cleanup'):
                steps += [step('cleanup'),step('refresh')]
                for e in entities.values():e['power']=e['toughness']=e['damage']=0;e['keys']={'Flying'} if e['name']=="Judge's Familiar" else set()

        if 'Shroud' in p['grants'] and entities[target]['alive']:
            protected='Shroud' in entities[target]['keys']
            for who in (0,1):steps.append(step('can_target_creature',ref=f'probe_{who}',other=target,value=int(not protected)))
            if entities['source']['alive'] and mode!='no_resource':
                steps.append(step('probe_activation_target',ref='source',other=target,index=-1 if ui['probe_dialog'] is None else ui['probe_dialog'],value=int(not protected)))
            # The own-only Oracle restriction is independent of any friendly
            # target protection and of the source's preferred-controller hint.
            if entities['source']['alive'] and any(entities[k]['alive'] for k in resources):
                steps.append(step('probe_activation_target',ref='source',other='enemy',index=-1,value=0))

        # The keyword is proved by legal combat and its actual damage result,
        # never by an engine-keyword bit or by the mere existence of a menu.
        probe=legal and mode not in ('mana_sibling','other_mode_only','probe_noncreature','probe_enemy') and entities[target]['alive'] and 'Shroud' not in p['grants']
        if probe:
            e=entities[target];w=entities['combat_witness'];keys=e['keys']
            steps += [step('turn',player=combat_turn,value=0),step('begin_attackers')]
            attacker='combat_witness' if defensive else target;blocker=target if defensive else 'combat_witness'
            can_attack=defensive or not e['tapped'] and (not e['sick'] or 'Haste' in keys)
            steps += [step('attack',ref=attacker,value=int(can_attack))]
            if can_attack:entities[attacker]['tapped']='Vigilance' not in entities[attacker]['keys']
            steps += [step('assert_card',ref=attacker,property='attacking',value=int(can_attack)),step('assert_card',ref=attacker,property='tapped',value=int(entities[attacker]['tapped'])),step('finish_attackers'),step('begin_blockers')]
            unblocked=probe_key in ('Double strike','Vigilance','Lifelink','Haste') and mode!='blocked'
            blocks=bool(can_attack and (not e['tapped'] and ('Flying' in keys or 'Reach' in keys or probe_key not in ('Flying','Reach')) if defensive else not unblocked and ('Flying' not in keys or 'Flying' in w['keys'] or 'Reach' in w['keys'])))
            if can_attack and not unblocked:steps += [step('block',ref=blocker,other=attacker,value=int(blocks))]
            steps += [step('advance_combat'),step('combat_damage'),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            if can_attack:
                # Damage is calculated in actual first-strike/normal packets.
                # A double striker participates in both while still alive;
                # a blocked nontrampler remains blocked after the blocker dies.
                a,b=attacker,blocker
                first=any(entities[k]['keys']&{'First strike','Double strike'} for k in (a,b))
                initial_first={k:bool(entities[k]['keys']&{'First strike','Double strike'}) for k in (a,b)}
                for stage in (('first','normal') if first else ('normal',)):
                    def participates(k):
                        return entities[k]['alive'] and (initial_first[k] if stage=='first' else not initial_first[k] or 'Double strike' in entities[k]['keys'])
                    packets=[]
                    if participates(a):
                        amount=max(0,stat(a,'power'))
                        if not blocks:packets.append((a,None,amount))
                        elif entities[b]['alive']:
                            lethal=1 if 'Deathtouch' in entities[a]['keys'] else max(0,stat(b,'toughness')-entities[b]['damage'])
                            to_blocker=min(amount,lethal) if 'Trample' in entities[a]['keys'] else amount
                            packets.append((a,b,to_blocker))
                            if amount>to_blocker:packets.append((a,None,amount-to_blocker))
                        elif 'Trample' in entities[a]['keys']:packets.append((a,None,amount))
                    if blocks and participates(b) and entities[a]['alive']:packets.append((b,a,max(0,stat(b,'power'))))
                    deathtouched=set()
                    for src,dst,amount in packets:
                        if 'Lifelink' in entities[src]['keys']:life[entities[src]['player']]+=amount
                        if dst is None:life[1-entities[src]['player']]-=amount
                        else:
                            entities[dst]['damage']+=amount
                            if amount and 'Deathtouch' in entities[src]['keys']:deathtouched.add(dst)
                    for key in (a,b):
                        if entities[key]['alive'] and (entities[key]['damage']>=stat(key,'toughness') or key in deathtouched):leave(key)
            steps += [step('end_combat'),step('assert_choices_consumed')]
        # Prove Flying in actual ground-block combat first, then pay the
        # second legal creature cost with the now-tapped source itself.
        if mode=='repeat_source' and entities['source']['alive']:steps+=activate('source')+resolve()
        final=check();steps+=final
        if mode=='payment' and legal:steps += [step('assert_mana',player=owner,color=c,value=0) for c in range(6)]
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending;tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('set_life',player=other,value=9),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        gaps=list(p['other_clauses'])
        if measured_victim:
            gaps+=['AI surviving-source Flying line is unexecuted: the measured automatic legal creature cost sacrifices the source, leaving Shield Sphere on its owner battlefield; subsequent source actions cannot occur. Human other-creature Flying controls remain separate.']
        if mode=='mana_sibling':gaps=[s for s in gaps if s!='{T}: Add {C}.']
        if p['land']:gaps+=['land-play history (source starts as a normal battlefield land fixture)']
        results.append(dict(name=f'ActivatedTemporaryModifierRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps+['control changes','blink identity','additional recipient native keywords']+(['Shield Sphere cost resource never blocks; Defender and its block-trigger counter stay dormant'] if sac in ('artifact','creature') and resources else []))))
    return results
