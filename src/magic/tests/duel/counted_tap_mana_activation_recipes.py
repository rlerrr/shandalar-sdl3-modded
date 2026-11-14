"""Pinned whole-Oracle mana counts, real entry payments and immediate receipts."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from printed_tap_mana_activation_recipes import casting_mana

FAMILY='counted_tap_mana_activation'
DATA=Path(__file__).parent/'data/counted_tap_mana_activation_wave8_profiles.json'
COLORS={'C':0,'B':1,'U':2,'G':3,'R':4,'W':5}


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mana_clause(line):
    m=re.fullmatch(r'(\{T\}|\{([23])\}, \{T\}): (.+)',line)
    if not m:raise ValueError('complete printed tap mana clause required')
    cost=int(m[2] or 0);text=m[3];color=None
    fixed=re.fullmatch(r'Add \{([CBG])\}\.',text)
    if fixed:kind='fixed';color=COLORS[fixed[1]]
    elif text=='Add {C} for each Locus on the battlefield.':kind='loci';color=0
    elif text=='Add {B} for each basic Swamp you control.':kind='basic_swamps';color=1
    elif text=='Add {B} for each black creature card in your graveyard.':kind='black_grave';color=1
    elif text=='Add X mana of any one color, where X is the number of Elves on the battlefield.':kind='elves'
    elif text=='Add X mana of any one color, where X is the number of Allies you control.':kind='allies'
    elif text=='Choose a color. Add one mana of that color for each charge counter on this artifact.':kind='charge'
    elif text=='Add {C} for each charge counter on this artifact.':kind='charge';color=0
    elif text=='Choose a color. Add an amount of mana of that color equal to your devotion to that color. (Your devotion to a color is the number of mana symbols of that color in the mana costs of permanents you control.)':kind='devotion'
    elif text=='Add an amount of {G} equal to your devotion to green. (Each {G} in the mana costs of permanents you control counts toward your devotion to green.)':kind='devotion';color=3
    else:
        u=re.fullmatch(r"Add \{C\}\. If you control an Urza's (Mine|Power-Plant) and an Urza's (Power-Plant|Tower), add (\{C\})((?:\{C\}){1,2}) instead\.",text)
        if not u:raise ValueError('unpiloted complete numeric mana effect')
        if u[1]==u[2]:raise ValueError('distinct printed Urza parts required')
        kind='urza';color=0
        return dict(cost=cost,kind=kind,color=color,parts=[u[1],u[2]],enhanced=1+u[4].count('{C}'))
    return dict(cost=cost,kind=kind,color=color)


def parse(record,ability):
    if not record or record.get('layout')!='normal' or record.get('card_faces') or not any(t in record.get('type_line','') for t in ('Land','Artifact','Creature')):raise ValueError('normal independent mana source required')
    lines=record['oracle_text'].splitlines();abilities={};native=[]
    for i,line in enumerate(lines):
        if line in ('This land enters tapped.','This artifact enters with X charge counters on it.','This artifact enters with a charge counter on it for each time it was kicked.') or line=='Multikicker {2} (You may pay an additional {2} any number of times as you cast this spell.)':native.append(line)
        else:abilities[i]=mana_clause(line)
    if type(ability) is not int or ability not in abilities:raise ValueError('integer printed ability required')
    entry='x' if 'This artifact enters with X charge counters on it.' in native else 'kicker' if any('for each time it was kicked' in s for s in native) else None
    if any(s.startswith('Multikicker') for s in native)!=(entry=='kicker'):raise ValueError('multikicker and entry count must compose')
    if entry=='x' and record['mana_cost']!='{X}{X}{X}' or entry=='kicker' and (record['mana_cost']!='{0}' or not any(s.startswith('Multikicker {2}') for s in native)):raise ValueError('complete real entry cost required')
    if any(p['kind']=='charge' for p in abilities.values())!=bool(entry):raise ValueError('charge output must compose with real entry')
    creature='Creature' in record['type_line']
    if creature and any(not re.fullmatch(r'[0-9]+',record.get(k,'')) for k in ('power','toughness')):raise ValueError('independent numeric source stats required')
    return dict(family=FAMILY,ability=ability,entry=entry,land='Land' in record['type_line'],creature=creature,legendary='Legendary' in record['type_line'],entry_tapped='This land enters tapped.' in native,siblings=[p for i,p in abilities.items() if i!=ability],other_clauses=[s for i,s in enumerate(lines) if i!=ability],**abilities[ability])


def variants(p):
    modes=['value','payment','inactive','tapped','repeat_denied','repeat_untap','spend','ready_snapshot','resolved_snapshot','opponent_turn']
    if p['kind'] not in ('fixed','charge'):modes+=['minimal_count','partial_count','wrong_owner','contrary_kind','dynamic_count']
    if p['kind']=='fixed':modes+=['fixed_zero_budget']
    if p['kind']=='charge':modes+=['dynamic_count']
    if p['color'] is None:modes+=['color_b','color_u','color_g','color_r','color_w']
    if p['cost']:modes+=['minimum_budget','short_budget_query']
    if p['creature']:modes+=['fresh']
    if p['entry_tapped']:modes+=['entry_tapped']
    if p['entry']:modes+=['entry_zero','entry_one','entry_three']
    if p['entry']=='kicker':modes+=['entry_decline']
    if p['entry']=='x':modes+=['xxx_cost_probe']
    if p['legendary']:modes+=['opponent_legend']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def spec(d,record):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2:raise ValueError('counted mana integer version2 required')
    q=d.get('params');s=pins()['sources'].get((record or {}).get('name'))
    if not s or s['oracle']!=record or d.get('oracle_text')!=record['oracle_text'] or not isinstance(q,dict) or set(q)!={'ability','interface','other_clauses'}:raise ValueError('whole independent source declaration required')
    p=parse(record,q['ability'])
    if s['abilities'].get(str(p['ability']))!=q['interface'] or q['other_clauses']!=p['other_clauses']:raise ValueError('printed clause or actual input drift')
    ui=q['interface']
    if set(ui)!={'mode_dialog','color_choice'} or ui['mode_dialog'] is not None and type(ui['mode_dialog']) is not int or ui['mode_dialog'] not in (None,0,1) or ui['color_choice'] not in ('none','output','after_payment'):raise ValueError('typed actual menu protocol required')
    if p['color'] is None and ui['color_choice']=='none' or p['color'] is not None and ui['color_choice']!='none':raise ValueError('printed selected color differs from presentation')
    if 'casting_x_inputs' in s:
        if p['entry']!='x' or not isinstance(s['casting_x_inputs'],dict) or set(s['casting_x_inputs'])!={'0','1'} or any(not isinstance(xs,list) or not xs or any(x!='declared' for x in xs) for xs in s['casting_x_inputs'].values()):raise ValueError('explicit real declared-X inputs required')
    if 'casting_pool_clicks' in s:
        clicks=s['casting_pool_clicks']
        if s['csvid']!=13670 or p['entry']!='x' or clicks!={'1':{'xxx_cost_probe_0':[0]}} or type(clicks['1']['xxx_cost_probe_0'][0]) is not int:raise ValueError('exact observed actor0 XXX probe pool click required')
    if 'casting_x_probe_inputs' in s:
        if s['csvid']!=13670 or p['entry']!='x' or s['casting_x_probe_inputs']!={'1':{'xxx_cost_probe_0':['declared','declared']}} or 'casting_pool_clicks' in s:raise ValueError('exact opt-in numeric helper probe required; no manual fallback plan')
    if 'casting_budget_x' in s:
        if s['csvid']!=13670 or p['entry']!='x' or type(s['casting_budget_x']) is not int or s['casting_budget_x']!=4 or s['casting_x_inputs']!={'0':['declared','declared'],'1':['declared']}:raise ValueError('measured XXX actor-specific intents required; Oracle fee remains3X')
    if 'zero_output_color_choice' in s and (s['csvid']!=13670 or s['zero_output_color_choice'] is not False):raise ValueError('only measured Cornucopia zero-output silent color input may be omitted')
    if any(q['kind']=='fixed' for q in [p]+p['siblings']):
        if type(s.get('fixed_sibling_wallet')) is not int or s['fixed_sibling_wallet']<max(q['cost'] for q in [p]+p['siblings']) or type(s.get('fixed_zero_dialog')) is not bool:raise ValueError('explicit bounded fixed-sibling presentation wallet required')
    if 'raw_ai_zero_query' in s:
        raw=s['raw_ai_zero_query']
        if not isinstance(raw,dict) or raw!=dict(raw_value=0,oracle_legal_activation=True,action='query_only',policy='AI_nonproductive_availability') or type(raw['raw_value']) is not int or raw['oracle_legal_activation'] is not True:raise ValueError('raw AI strategy must not alter Oracle zero-output legality')
    if 'fixed_output_choices' in s:
        choices=s['fixed_output_choices'];modes=('opponent_legend','opponent_turn','payment','ready_snapshot','repeat_denied','repeat_untap','resolved_snapshot','spend','value')
        wanted={'0':{mode+'_'+str(who):0 for mode in modes for who in (0,1)}}
        if s['csvid']!=13494 or choices!=wanted or any(type(c) is not int for c in choices['0'].values()):raise ValueError('exact observed Nykthos fixed-C palette inputs required')
    if 'ai_mode_dialog' in s and (s['csvid']!=18365 or s['ai_mode_dialog'] is not False):raise ValueError('only observed Cabal AI unoffered mode plan may be omitted')
    if 'ai_free_fixture' in s:
        wanted={'variants':[mode+'_1' for mode in ('fixed_zero_budget','opponent_turn','payment','ready_snapshot','repeat_denied','repeat_untap','resolved_snapshot','spend','value')],'floating_colorless':0,'other_own_lands_tapped':True}
        f=s['ai_free_fixture']
        if s['csvid']!=18365 or f!=wanted or type(f['floating_colorless']) is not int or f['other_own_lands_tapped'] is not True or s.get('ai_mode_dialog') is not False:raise ValueError('exact isolated Cabal free-AI fixture required')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or any(type(v) is not str for v in vs) or len(set(vs))!=len(vs) or not set(vs)<=set(variants(p)):raise ValueError('distinct explicit supported variants required')
    return p


def devotion(record,color):
    symbol='CBUGRW'[color]
    return sum(symbol in token.split('/') for token in re.findall(r'\{([^}]+)\}',record.get('mana_cost','')))


def mana_amount(p,entities,oracle,owner,color):
    live=[e for e in entities.values() if e['zone']=='battlefield']
    def matches(e,typ,sub=None):
        text=oracle[e['name']]['type_line'];types,_,subs=text.partition(' — ')
        return typ in types.split() and (sub is None or sub in subs.split())
    def subtype(e,sub):return sub in oracle[e['name']]['type_line'].partition(' — ')[2].split()
    kind=p['kind']
    if kind=='fixed':return 1
    if kind=='loci':return sum(matches(e,'Land','Locus') for e in live)
    if kind=='elves':return sum(subtype(e,'Elf') for e in live)
    if kind=='allies':return sum(e['player']==owner and subtype(e,'Ally') for e in live)
    if kind=='basic_swamps':return sum(e['player']==owner and matches(e,'Land','Swamp') and 'Basic' in oracle[e['name']]['type_line'].split(' — ')[0].split() for e in live)
    if kind=='black_grave':return sum(e['player']==owner and e['zone']=='graveyard' and 'Creature' in oracle[e['name']]['type_line'] and 'B' in oracle[e['name']]['colors'] for e in entities.values())
    if kind=='devotion':return sum(devotion(oracle[e['name']],color) for e in live if e['player']==owner)
    if kind=='charge':return entities['source']['charge']
    if kind=='urza':
        return p['enhanced'] if all(any(e['player']==owner and matches(e,'Land',part) and "Urza's" in oracle[e['name']]['type_line'].split(' — ')[-1].split() for e in live) for part in p['parts']) else 1
    raise ValueError('unknown count expression')


def validate_witnesses(oracle,registry):
    for n,w in pins()['witnesses'].items():
        row=registry.get(n,{})
        if oracle.get(n)!=w['oracle'] or row.get('csvid')!=w['csvid'] or row.get('handler')!=w['handler'] or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy') or w['oracle']['layout']!='normal':raise ValueError('independent normal implemented witness changed: '+n)


def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step
    data=pins();src=data['sources'][name];r=registry.get(name,{})
    if r.get('csvid')!=csvid or src['csvid']!=csvid or r.get('handler')!=src['handler'] or r.get('kind')!='card' or r.get('handler') in (None,'NULL','0','card_dummy'):raise ValueError('normal implemented primary required')
    validate_witnesses(oracle,registry)
    results=[]
    for variant in selected:
        mode,actor=variant.rsplit('_',1);owner=int(actor);other=1-owner;setup=[];entities={};steps=[];pools=[[0]*6 for _ in (0,1)];life=[20,20]
        free_ai_fixture=p['kind']=='fixed' and variant in src.get('ai_free_fixture',{}).get('variants',[])
        color=COLORS[mode[-1].upper()] if mode.startswith('color_') else p['color'] if p['color'] is not None else 2
        chosen_x=0 if mode=='entry_zero' else 1 if mode=='entry_one' else 3 if mode=='entry_three' else 2
        def add(n,key,player,zone='battlefield',tapped=False):
            if n!=name and n not in data['witnesses']:raise ValueError('unpinned fixture: '+n)
            if free_ai_fixture and key!='source' and player==owner and zone=='battlefield' and 'Land' in oracle[n]['type_line']:tapped=True
            c=card(n,key,player,zone)
            if zone=='battlefield':c['state']=['tapped' if tapped else 'untapped']
            setup.append(c);entities[key]=dict(name=n,player=player,zone=zone,tapped=tapped,sick=False,damage=0,charge=0)
        def emit(op,**kw):steps.append(step(op,**kw))
        def fund(player,values):
            pools[player]=list(values)
            for c,v in enumerate(values):emit('set_mana',player=player,color=c,value=v)
        def creature(e):return 'Creature' in oracle[e['name']]['type_line']
        def check():
            emit('assert_stack',value=0);emit('assert_choices_consumed');names={e['name'] for e in entities.values()}
            for key,e in entities.items():
                if e['zone']!='battlefield':continue
                emit('require_card',ref=key,property='in_play',value=1);emit('assert_card',ref=key,property='tapped',value=int(e['tapped']))
                if key=='source' and p['entry']:emit('assert_counter',ref=key,counter_kind='charge',value=e['charge'])
                if creature(e):
                    for k in ('power','toughness'):emit('require_card',ref=key,property=k,value=int(oracle[e['name']][k]))
                    emit('assert_card',ref=key,property='sick',value=int(e['sick']));emit('assert_card',ref=key,property='damage',value=e['damage'])
            for who in (0,1):
                emit('assert_life',player=who,value=life[who]);emit('assert_mana_total',player=who,value=sum(pools[who]))
                for c,v in enumerate(pools[who]):emit('assert_mana',player=who,color=c,value=v)
                for z in ('hand','battlefield','graveyard','library','exile'):
                    counts=Counter(e['name'] for e in entities.values() if e['player']==who and e['zone']==z)
                    emit('assert_zone',player=who,zone=z,value=sum(counts.values()))
                    for n in sorted(names):emit('assert_zone_multiplicity',player=who,zone=z,index=registry[n]['csvid'],value=counts[n])
        # Predeclare every later spell/witness before any receipt is emitted.
        add('Scion of Glaciers','enemy',other);add('Icy Manipulator','spender',owner)
        fixture_kind=p['kind'] if p['kind']!='fixed' else next(q['kind'] for q in p['siblings'] if q['kind']!='fixed')
        minimal=mode=='minimal_count';partial=mode=='partial_count';wrong=mode=='wrong_owner';contrary=mode=='contrary_kind'
        if fixture_kind=='loci':
            if not minimal:add('Cloudpost','support_a',other if wrong else owner)
            if not minimal and not partial:add('Forest' if contrary else 'Cloudpost','support_b',other)
        elif fixture_kind=='urza':
            parts=next(q['parts'] for q in [p]+p['siblings'] if q['kind']=='urza');names={'Mine':"Urza's Mine",'Power-Plant':"Urza's Power Plant",'Tower':"Urza's Tower"}
            if not minimal:add(names[parts[0]],'support_a',owner)
            if not minimal and not partial:add(names[parts[0]] if contrary else names[parts[1]],'support_b',other if wrong else owner)
        elif fixture_kind=='basic_swamps':
            for i in range(0 if minimal or contrary else 1 if partial else 3):add('Swamp','support_'+str(i),other if wrong else owner)
            if not minimal:add('Badlands','nonbasic_swamp',owner);add('Island','nonmatching_land',owner)
        elif fixture_kind=='black_grave':
            for i in range(0 if minimal or contrary else 1 if partial else 3):add('Vesper Ghoul','support_'+str(i),other if wrong else owner,'graveyard')
            if not minimal:add('Terror','black_noncreature',owner,'graveyard');add('Sylvan Safekeeper','nonblack_creature',owner,'graveyard')
            add('Vesper Ghoul','black_live',owner)
        elif fixture_kind=='elves':
            if not minimal:add('Elvish Mystic','support_a',other if wrong else owner)
            if not minimal and not partial:add('Sylvan Safekeeper' if contrary else 'Elvish Mystic','support_b',other)
        elif fixture_kind=='allies':
            if not minimal:add('Harabaz Druid','support_a',other if wrong else owner)
            if not minimal and not partial:add('Elvish Mystic' if contrary else 'Harabaz Druid','support_b',other if wrong else owner)
            add('Harabaz Druid','foreign_ally',other)
        elif fixture_kind=='devotion':
            if p['color']==3:
                if not minimal:add('Elvish Mystic','support_a',other if wrong else owner)
                if not minimal and not partial:add("Judge's Familiar" if contrary else 'Elvish Hexhunter','support_b',other if wrong else owner)
                add('Scion of Glaciers','other_color',owner)
            else:
                if not minimal:add('Sylvan Safekeeper' if contrary else 'Scion of Glaciers','support_a',other if wrong else owner)
                if not minimal and not partial:add("Judge's Familiar",'support_b',other if wrong else owner)
                add('Elvish Mystic','other_color',owner)
        if mode=='opponent_legend':add(name,'other_legend',other)
        if mode=='dynamic_count' and p['kind']=='charge':add('Vorel of the Hull Clade','counter_doubler',other)
        if mode in ('repeat_untap','dynamic_count'):add('Twiddle','untap',other,'hand')
        if p['entry_tapped'] and mode!='entry_tapped':add('Twiddle','entry_untap',other,'hand')
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        for who in (0,1):
            add('Forest','hand_'+str(who),who,'hand');add('Plains','grave_'+str(who),who,'graveyard');add('Island','library_'+str(who),who,'library');add('Mountain','exile_'+str(who),who,'exile')
        add(name,'source',owner,'battlefield' if p['land'] else 'hand',p['entry_tapped'])
        emit('reset_death_history');emit('turn',player=owner,value=0);emit('present_ai_dialogs',value=1);fund(0,[0]*6);fund(1,[0]*6);emit('refresh')
        if not p['land']:
            vector=[0]*6
            if p['entry']=='x':vector[0]=12 if mode=='xxx_cost_probe' else src.get('casting_budget_x',3)*chosen_x
            elif p['entry']=='kicker':vector[0]=2*chosen_x+2*int(mode=='entry_decline' or mode=='entry_zero' and owner==1)
            else:vector=casting_mana(oracle[name])
            fund(owner,vector)
            if p['entry']=='x':
                x_inputs=src.get('casting_x_probe_inputs',{}).get(str(p['ability']),{}).get(variant,src['casting_x_inputs'][str(owner)])
                for token in x_inputs:emit('choose_x',player=owner,value=chosen_x if token=='declared' else token)
                for pool_color in src.get('casting_pool_clicks',{}).get(str(p['ability']),{}).get(variant,[]):emit('choose_mana',player=owner,color=pool_color)
            if p['entry']=='kicker':
                for _ in range(chosen_x):emit('choose_dialog',player=owner,value=src['multikicker_inputs']['pay'])
                if vector[0]>2*chosen_x:emit('choose_dialog',player=owner,value=src['multikicker_inputs']['pass'])
            emit('cast',ref='source',value=1);emit('assert_stack',value=1)
            total=3*chosen_x if p['entry']=='x' else 2*chosen_x if p['entry']=='kicker' else sum(vector)
            pools[owner][0]-=total if p['entry'] else 0
            if not p['entry']:pools[owner]=[0]*6
            for c,v in enumerate(pools[owner]):emit('assert_mana',player=owner,color=c,value=v)
            emit('resolve');emit('refresh');emit('assert_choices_consumed');entities['source']['zone']='battlefield';entities['source']['sick']=p['creature'];entities['source']['charge']=chosen_x if p['entry'] else 0
            check()
            if p['creature'] and mode!='fresh':emit('advance_turn');emit('advance_turn');entities['source']['sick']=False
        def untap(key):
            # Opponent's real instant; no source-controller priority invention.
            emit('choose_target',player=other,ref='source');emit('cast',ref=key,value=1);emit('choose_dialog',player=other,value=1);emit('resolve');emit('refresh');emit('assert_choices_consumed')
            pools[other][2]-=1;entities[key]['zone']='graveyard';entities['source']['tapped']=False
        blue=int('untap' in entities)+int('entry_untap' in entities);v=[0]*6;v[2]=blue+int('counter_doubler' in entities);v[3]=int('counter_doubler' in entities);v[0]=int(mode=='tapped');fund(other,v)
        if p['entry_tapped']:
            emit('require_card',ref='source',property='tapped',value=1)
            if mode!='entry_tapped':untap('entry_untap')
        # Never erase the cost-probe's measured residual before its final receipt.
        if mode!='xxx_cost_probe':
            wallet=[0]*6;wallet[0]=p['cost']*(2 if mode in ('repeat_untap','dynamic_count') else 1)+(2 if mode=='value' else 0)
            if p['kind']=='fixed':wallet[0]=src['fixed_sibling_wallet']
            if free_ai_fixture:wallet[0]=src['ai_free_fixture']['floating_colorless']
            if mode=='fixed_zero_budget':wallet[0]=0
            if mode=='short_budget_query':wallet[0]=max(0,p['cost']-1)
            fund(owner,wallet)
        emit('turn',player=other if mode=='opponent_turn' else owner,value=0);emit('refresh')
        if mode=='tapped':
            emit('choose_target',player=other,ref='source');emit('activate',ref='tapper',value=1);emit('assert_stack',value=1);emit('resolve');emit('refresh');emit('assert_choices_consumed');pools[other][0]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        ready=len(steps)
        def activate():
            emit('require_can_activate',ref='source',value=1)
            ui=d['params']['interface']
            if not(owner==1 and src.get('ai_mode_dialog') is False) and ui['mode_dialog'] is not None and (p['cost'] or p['kind']=='fixed' and (mode!='fixed_zero_budget' or src['fixed_zero_dialog'])):emit('choose_dialog',player=owner,value=ui['mode_dialog'])
            if p['color'] is None and not(src.get('zero_output_color_choice') is False and mana_amount(p,entities,oracle,owner,color)==0):emit('choose_color',player=owner,value=color)
            elif variant in src.get('fixed_output_choices',{}).get(str(p['ability']),{}):emit('choose_color',player=owner,value=src['fixed_output_choices'][str(p['ability'])][variant])
            if pools[owner][0]<p['cost']:raise ValueError('actual generic pool must pay the declared cost')
            emit('activate',ref='source',value=1);pools[owner][0]-=p['cost'];entities['source']['tapped']=True;pools[owner][color]+=mana_amount(p,entities,oracle,owner,color);check()
        denied=mode in ('tapped','entry_tapped') or mode=='fresh' and p['creature']
        if mode=='xxx_cost_probe':pass
        elif mode=='short_budget_query':emit('require_can_activate',ref='source',value=1)
        elif denied:emit('require_can_activate',ref='source',value=0)
        elif mode=='entry_zero' and owner==1 and src.get('raw_ai_zero_query'):emit('can_activate',ref='source',value=src['raw_ai_zero_query']['raw_value'])
        elif mode!='inactive':
            activate()
            if mode=='repeat_denied':emit('require_can_activate',ref='source',value=0)
            if mode in ('repeat_untap','dynamic_count'):
                if mode=='dynamic_count':
                    if p['kind']=='charge':
                        emit('choose_target',player=other,ref='source');emit('activate',ref='counter_doubler',value=1);emit('assert_stack',value=1);emit('resolve');emit('refresh');emit('assert_choices_consumed');entities['counter_doubler']['tapped']=True;entities['source']['charge']*=2;pools[other][2]-=1;pools[other][3]-=1
                    else:
                        key='black_live' if p['kind']=='black_grave' else 'support_b' if 'support_b' in entities else 'support_0' if 'support_0' in entities else 'support_a' if 'support_a' in entities else None
                        if key:emit('destroy',ref=key);emit('refresh');entities[key]['zone']='graveyard'
                untap('untap')
                activate()
            if mode=='spend':
                pool=next(c for c,n in enumerate(pools[owner]) if n)
                if owner==0 and pool:emit('choose_mana',player=owner,color=pool)
                emit('choose_target',player=owner,ref='enemy');emit('activate',ref='spender',value=1);emit('assert_stack',value=1);emit('resolve');emit('refresh');pools[owner][pool]-=1;entities['spender']['tapped']=True;entities['enemy']['tapped']=True;check()
        final_start=len(steps);check();final=copy.deepcopy(steps[final_start:]);tail=copy.deepcopy(steps[ready:])
        if mode=='ready_snapshot':steps=steps[:ready]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('set_life',player=other,value=9),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        gaps=['modified/replacement production and restricted mana provenance','control changes and type/text copying']
        if p['land']:gaps+=['Legal land-play history is unavailable; battlefield land fixtures preserve printed tapped state and use real Twiddle readiness, not land spell casting.']
        if mode=='short_budget_query':gaps+=['Only globally legal fixed sibling availability is queried; unselected paid counted-mode affordability is not inferred.']
        if p['legendary']:gaps+=['Same-controller duplicate legendary-land selection is unexecuted; singleton and opposing-controller copy remain distinct.']
        meta=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)
        meta['oracle_amount_expression']={k:copy.deepcopy(p[k]) for k in ('kind','color','cost','parts','enhanced') if k in p}
        if p['entry']:meta['entry_receipt']=dict(declared_value=chosen_x,printed_total=3*chosen_x if p['entry']=='x' else 2*chosen_x,kind=p['entry'])
        if mode=='entry_zero' and owner==1 and src.get('raw_ai_zero_query'):meta['raw_ai_availability']=copy.deepcopy(src['raw_ai_zero_query'])
        results.append(dict(name=f'CountedTapManaActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=meta))
    return results
