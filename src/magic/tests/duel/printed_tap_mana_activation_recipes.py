"""Whole pinned tap-mana sources: real costs, immediate output and native clauses."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

FAMILY='printed_tap_mana_activation'
DATA=Path(__file__).parent/'data/printed_tap_mana_activation_wave8_profiles.json'
COLORS={'C':0,'B':1,'U':2,'G':3,'R':4,'W':5}
BASICS={0:'Wastes',1:'Swamp',2:'Island',3:'Forest',4:'Mountain',5:'Plains'}
OUTPUTS={
 'one mana of any color':'any_color',
 'one mana of the chosen color':'chosen_color',
 'one mana of any color that a land an opponent controls could produce':'opponent_colors',
 'one mana of any color that a basic land you control could produce':'own_basic_colors',
 'one mana of any type that a land you control could produce':'own_types'}
NATIVE={'Reach (This creature can block creatures with flying.)','This artifact enters tapped.',
 'As this artifact enters, choose a color.','As this creature enters, choose a color.'}

def pins():return json.loads(DATA.read_text(encoding='utf8'))

def casting_mana(record):
    text=record.get('mana_cost','');out=[0]*6
    if not re.fullmatch(r'(?:\{(?:\d+|[WUBRG])\})*',text):raise ValueError('ordinary printed casting cost required')
    for token in re.findall(r'\{([^}]+)\}',text):out[0 if token.isdigit() else COLORS[token]]+=int(token) if token.isdigit() else 1
    return out

def clause(line):
    line=line.removesuffix(' ({C} represents colorless mana.)')
    if ': ' not in line:raise ValueError('whole mana clause required')
    prefix,effect=line.split(': ',1)
    costs={'{T}':(0,0,False),'{1}, {T}':(1,0,False),'{T}, Pay 1 life':(0,1,False),
           '{T}, Tap an untapped creature you control':(0,0,True)}
    if prefix not in costs:raise ValueError('unpiloted typed mana cost')
    gain=effect.endswith(' You gain 1 life.')
    if gain:effect=effect[:-len(' You gain 1 life.')]
    m=re.fullmatch(r'Add (.*)\.',effect)
    if not m:raise ValueError('complete immediate mana effect required')
    fixed=re.fullmatch(r'\{([CWUBRG])\}',m[1]);kind='fixed' if fixed else OUTPUTS.get(m[1])
    if not kind:raise ValueError('unpiloted output expression')
    generic,life,other=costs[prefix]
    return dict(generic=generic,pay_life=life,tap_other=other,output=kind,fixed_color=COLORS[fixed[1]] if fixed else None,gain_life=int(gain))

def parse(record,ability):
    if not record or record.get('layout')!='normal' or record.get('card_faces') or any(t in record.get('type_line','') for t in ('Token','Aura','Equipment','Planeswalker')):raise ValueError('whole normal standalone source required')
    lines=record['oracle_text'].splitlines()
    if type(ability) is not int or not 0<=ability<len(lines):raise ValueError('integer selected printed clause required')
    mana_lines={}
    for i,line in enumerate(lines):
        if line not in NATIVE:mana_lines[i]=clause(line)
    if ability not in mana_lines:raise ValueError('selected mana activation required')
    creature='Creature' in record['type_line'];land='Land' in record['type_line']
    if creature and (any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness')) or int(record['toughness'])<=0):raise ValueError('independent numeric living source required')
    chosen=any('choose a color' in line for line in lines)
    if any(v['output']=='chosen_color' for v in mana_lines.values())!=chosen:raise ValueError('entry choice and output must compose')
    return dict(family=FAMILY,ability=ability,creature=creature,land=land,entry_tapped='This artifact enters tapped.' in lines,entry_choice=chosen,reach=any(s.startswith('Reach ') for s in lines),other_clauses=[s for i,s in enumerate(lines) if i!=ability],siblings=[v for i,v in mana_lines.items() if i!=ability],**mana_lines[ability])

def variants(p):
    v=['value','payment','inactive','tapped','repeat_denied','repeat_untap','spend','cleanup','ready_snapshot','resolved_snapshot','opponent_turn']
    if not p['generic']:v+=['untap_step']
    if p['creature']:v+=['fresh']
    if p['entry_tapped']:v+=['entry_tapped']
    if p['output']!='fixed':v+=['color_'+c.lower() for c in 'BUGRW']
    if p['output']=='own_types':v+=['color_c']
    if p['output'] in ('opponent_colors','own_basic_colors','own_types'):v+=['palette_choice','empty_query','wrong_owner_query']
    if p['output']=='own_basic_colors':v+=['nonbasic_query']
    if p['output'] in ('opponent_colors','own_basic_colors'):v+=['zero_output']
    if p['generic']:v+=['filter_unpayable_query']
    if p['pay_life']:v+=['life_two','life_one_query']
    if p['tap_other']:v+=['no_helper','helper_tapped','helper_fresh','helper_shroud']
    if p['reach']:v+=['reach']
    if p['entry_choice'] and p['output']=='fixed' and any(s['output']=='chosen_color' for s in p['siblings']):v+=['entry_other_color']
    if p['output']=='fixed' and p['fixed_color']==0 and any(s['tap_other'] for s in p['siblings']):v+=['free_with_helper']
    return [item+'_'+str(owner) for owner in (0,1) for item in v]

def spec(d,record):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2:raise ValueError('tap-mana family/integer version2 required')
    q=d.get('params')
    if not isinstance(q,dict) or set(q)!={'ability','interface','other_clauses'} or d.get('oracle_text')!=(record or {}).get('oracle_text'):raise ValueError('complete independent Oracle and interface required')
    p=parse(record,q['ability']);s=pins()['sources'].get(record['name'])
    if not s or s['oracle']!=record or s['abilities'].get(str(p['ability']))!=q['interface'] or q['other_clauses']!=p['other_clauses']:raise ValueError('independent printed source/interface drift')
    ui=q['interface']
    if set(ui)!={'entry_choice','output_choice','cost_creature_choices','cost_before_color','zero_output_no_color','filter_menu'} or type(ui['entry_choice']) is not bool or ui['entry_choice']!=p['entry_choice'] or ui['output_choice']!='when_multiple' or any(type(ui[k]) is not bool for k in ('cost_before_color','zero_output_no_color','filter_menu')):raise ValueError('actual typed source presentation required')
    if ui['filter_menu'] and not any(clause(s)['generic'] for s in record['oracle_text'].splitlines() if s not in NATIVE):raise ValueError('filter presentation requires printed generic filter')
    if not isinstance(ui['cost_creature_choices'],dict) or set(ui['cost_creature_choices'])!={'0','1'} or any(type(v) is not bool for v in ui['cost_creature_choices'].values()):raise ValueError('typed actual creature-cost input required')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or any(type(v) is not str for v in vs) or len(set(vs))!=len(vs) or not set(vs)<=set(variants(p)):raise ValueError('distinct explicit supported variants required')
    validate_fixture_inputs(p,s)
    return p

def validate_fixture_inputs(p,source):
    # These are measured presentation/raw-policy receipts. They never change
    # an Oracle cost, production rule, eligible creature or zero-output legality.
    presentation=source.get('append_presentation',{})
    if not isinstance(presentation,dict) or not set(presentation)<={'combined_entry_palette','creature_color_menu','shrouded_helper_query_only'} or any(type(v) is not bool for v in presentation.values()):raise ValueError('bounded append-only source presentation required')
    clauses=[p]+p['siblings']
    if presentation.get('combined_entry_palette') and not(p['entry_choice'] and any(s['output']=='fixed' for s in clauses) and any(s['output']=='chosen_color' for s in clauses)):raise ValueError('printed fixed and entry-chosen mana siblings required')
    if presentation.get('creature_color_menu') and not(p['land'] and any(s['output']=='fixed' and s['fixed_color']==0 for s in clauses) and any(s['tap_other'] for s in clauses)):raise ValueError('printed freeC and creature-cost siblings required')
    if presentation.get('shrouded_helper_query_only') and not presentation.get('creature_color_menu'):raise ValueError('protected-cost selector gap requires a separate freeC observation')
    sacrifices=source.get('observed_ai_sacrifices',{})
    if 'observed_ai_sacrifices' in source and (source['csvid']!=17234 or sacrifices!={'1':{'helper_shroud_1':'source'}} or not presentation.get('shrouded_helper_query_only')):raise ValueError('exact measured Safekeeper AI source-sacrifice fixture required')
    by_ability=source.get('generic_payment_inputs',{})
    if not isinstance(by_ability,dict) or not set(by_ability)<=set(source['abilities']) or any(not isinstance(v,dict) for v in by_ability.values()):raise ValueError('typed measured generic-payment inputs by printed ability required')
    payments=by_ability.get(str(p['ability']),{})
    for variant,roles in payments.items():
        if variant not in variants(p) or not variant.endswith('_0') or not isinstance(roles,dict) or len(roles)!=1:raise ValueError('exact measured human payment variant required')
        role,color=next(iter(roles.items()))
        if type(color) is not int or not 1<=color<=5:raise ValueError('actual positive colored pool click required')
        if role=='spender':
            if variant.rsplit('_',1)[0] not in ('spend','cleanup','untap_step','reach'):raise ValueError('measured spender input required')
        elif role=='source_activation_2':
            if variant!='repeat_untap_0' or p['generic']!=1:raise ValueError('measured second filter payment required')
        else:raise ValueError('unsupported measured payment operation')
    policies=source.get('raw_ai_query_policy',{})
    if not isinstance(policies,dict):raise ValueError('typed raw AI query observations required')
    for variant,policy in policies.items():
        if variant not in variants(p) or variant not in ('empty_query_1','wrong_owner_query_1','nonbasic_query_1','zero_output_1') or p['output'] not in ('opponent_colors','own_basic_colors'):raise ValueError('nonproductive raw AI query scope required')
        if (not isinstance(policy,dict) or set(policy)!={'raw_value','oracle_legal_activation','zero_output','action','policy'}
                or type(policy['raw_value']) is not int or policy['raw_value']!=0 or policy['oracle_legal_activation'] is not True or policy['zero_output'] is not True
                or policy['action']!='query_only' or policy['policy']!='AI_nonproductive_availability'):raise ValueError('raw policy must remain separate from Oracle legal1')

def validate_witnesses(oracle,registry):
    for n,s in pins()['witnesses'].items():
        r=registry.get(n,{})
        if r.get('csvid')!=s['csvid'] or r.get('kind')!='card' or r.get('handler')!=s['handler'] or r.get('handler') in (None,'NULL','0','card_dummy') or oracle.get(n)!=s['oracle']:raise ValueError('normal implemented independent witness changed: '+n)

def expand(name,csvid,d,p,selected,registry,oracle):
    from recipes import card,step
    src=pins()['sources'][name];row=registry.get(name,{})
    if row.get('kind')!='card' or row.get('csvid')!=csvid or src['csvid']!=csvid or row.get('handler')!=src['handler'] or row.get('handler') in (None,'NULL','0','card_dummy'):raise ValueError('implemented primary identity required')
    validate_witnesses(oracle,registry);results=[];ui=d['params']['interface']
    for variant in selected:
        mode,who=variant.rsplit('_',1);owner=int(who);other=1-owner;setup=[];entities={};steps=[]
        zones={z:[[],[]] for z in ('hand','graveyard','library','exile')};life=[20,20];pools=[[0]*6 for _ in (0,1)]
        payment_inputs=src.get('generic_payment_inputs',{}).get(str(p['ability']),{}).get(variant,{})
        raw_policy=src.get('raw_ai_query_policy',{}).get(variant)
        sacrificed_source=src.get('observed_ai_sacrifices',{}).get(str(p['ability']),{}).get(variant)=='source'
        activation_number=0
        color=COLORS[mode[-1].upper()] if mode.startswith('color_') else 4 if p['output']=='chosen_color' else 2 if p['output']=='opponent_colors' else 0 if p['output']=='own_types' or p['output']=='fixed' else 3
        if p['output']=='fixed':color=p['fixed_color']
        entry_color=2 if mode=='entry_other_color' else color
        presentation=src.get('append_presentation',{})
        if mode=='palette_choice':color=4 if p['output']=='opponent_colors' else 1 if p['output']=='own_basic_colors' else 0
        palette=set(range(1,6)) if p['output']=='any_color' else {color}
        supports=[];has_palette=p['output'] in ('opponent_colors','own_basic_colors','own_types')
        if has_palette:
            side=other if p['output']=='opponent_colors' else owner
            if mode in ('empty_query','zero_output'):palette=set()
            elif mode=='wrong_owner_query':supports=[(BASICS[color],1-side)];palette=set()
            elif mode=='nonbasic_query':supports=[('Mana Confluence',owner)];palette=set()
            else:
                supports=[(BASICS[color],side)]
                if mode=='palette_choice':palette={color,2};supports.append(('Island',side))
                # A different controller's unrelated land cannot leak a color.
                supports.append(('Forest' if color!=3 else 'Island',1-side))
        if mode=='filter_unpayable_query':palette={color}
        if raw_policy and palette:raise ValueError('raw nonproductive query requires independent empty palette')

        def add(n,key,player,zone='battlefield'):
            if n!=name and n not in pins()['witnesses']:raise ValueError('unpinned source fixture')
            c=card(n,key,player,zone)
            if zone=='battlefield':c['state']=['untapped']
            setup.append(c);entities[key]=dict(name=n,player=player,alive=zone=='battlefield',tapped=False,sick=False,damage=0)
            if zone!='battlefield':zones[zone][player].append(n)
        def is_creature(key):return 'Creature' in oracle[entities[key]['name']]['type_line']
        def leave(key,z='graveyard'):
            e=entities[key];e['alive']=False;zones[z][e['player']].append(e['name'])
        def fund(player,vector):return [step('set_mana',player=player,color=i,value=v) for i,v in enumerate(vector)]
        def check(stack=0):
            s=[step('assert_stack',value=stack),step('assert_choices_consumed')];names={e['name'] for e in entities.values()}
            for key,e in entities.items():
                if not e['alive']:continue
                s += [step('require_card',ref=key,property='in_play',value=1),step('assert_card',ref=key,property='tapped',value=int(e['tapped']))]
                if is_creature(key):s += [step('require_card',ref=key,property=k,value=int(oracle[e['name']][k])) for k in ('power','toughness')]+[step('assert_card',ref=key,property='sick',value=int(e['sick'])),step('assert_card',ref=key,property='damage',value=e['damage'])]
            for player in (0,1):
                s += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(pools[player]))]+[step('assert_mana',player=player,color=i,value=v) for i,v in enumerate(pools[player])]
                s += [step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values()))]+[step('assert_zone_multiplicity',player=player,zone='battlefield',index=registry[n]['csvid'],value=sum(e['alive'] and e['player']==player and e['name']==n for e in entities.values())) for n in sorted(names)]
                for z in zones:
                    ns=zones[z][player];counts=Counter(ns);s += [step('assert_zone',player=player,zone=z,value=len(ns))]
                    if z=='library':s += [step('assert_zone_card',player=player,zone=z,index=i,value=registry[n]['csvid']) for i,n in enumerate(ns)]
                    else:s += [step('assert_zone_multiplicity',player=player,zone=z,index=registry[n]['csvid'],value=counts[n]) for n in sorted(names)]
            return s
        for i,(n,player) in enumerate(supports):add(n,'support_'+str(i),player)
        add('Scion of Glaciers','enemy',other);add('Icy Manipulator','spender',owner)
        if (p['tap_other'] or mode=='free_with_helper') and mode!='no_helper':add('Scion of Glaciers','helper',owner,'hand' if mode=='helper_fresh' else 'battlefield')
        if p['reach']:add("Judge's Familiar",'flyer',other)
        twiddles=[]
        if mode in ('repeat_untap','reach'):
            for i in range(2 if p['tap_other'] else 1):add('Twiddle','untap_'+str(i),other,'hand');twiddles.append('untap_'+str(i))
        if p['entry_tapped'] and mode!='entry_tapped':add('Twiddle','entry_untap',other,'hand')
        if mode in ('tapped','helper_tapped'):add('Icy Manipulator','tapper',other)
        if mode=='helper_shroud':add('Sylvan Safekeeper','protector',owner);add('Plains','sac_land',owner)
        for player in (0,1):
            for z,n in (('hand','Forest'),('graveyard','Plains'),('library','Island'),('exile','Mountain')):add(n,z+'_'+str(player),player,z)
        add(name,'source',owner,'battlefield' if p['land'] else 'hand')
        # Establish the initial fixture budget before any real cast or turn.
        # In particular, do not carry empty_main's other-player mana through
        # guarded turn/untap preparation, or erase measured production later.
        steps=[step('reset_death_history'),step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]+fund(0,[0]*6)+fund(1,[0]*6)+[step('refresh')]

        def cast(key):
            e=entities[key];player=e['player'];n=e['name'];vector=casting_mana(oracle[n])
            seq=fund(player,vector)+[step('cast',ref=key,value=1),step('assert_stack',value=1)]
            if key=='source' and ui['entry_choice']:seq.append(step('choose_color',player=player,value=entry_color))
            seq += [step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=player,value=0)]
            e['alive']=True;e['sick']=is_creature(key);zones['hand'][player].remove(n)
            if key=='source' and p['entry_tapped']:e['tapped']=True;seq.append(step('require_card',ref=key,property='tapped',value=1))
            return seq
        if not p['land']:
            steps+=cast('source')
            if p['creature'] and mode!='fresh':steps += [step('advance_turn'),step('advance_turn')];entities['source']['sick']=False
        if mode=='helper_fresh':steps += [step('turn',player=owner,value=0)]+cast('helper')
        def twiddle(key,target):
            seq=[step('choose_target',player=other,ref=target),step('cast',ref=key,value=1),step('choose_dialog',player=other,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            zones['hand'][other].remove('Twiddle');zones['graveyard'][other].append('Twiddle');pools[other][2]-=1;entities[target]['tapped']=False
            return seq
        if p['entry_tapped'] and mode!='entry_tapped':
            pools[other][2]=1;steps += fund(other,pools[other])+twiddle('entry_untap','source');steps.append(step('assert_mana_total',player=other,value=0))
        if mode=='helper_shroud':
            steps += [step('choose_target',player=owner,ref='helper')]
            if owner==0:steps.append(step('choose_target',player=owner,ref='sac_land'))
            steps += [step('activate',ref='protector',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')];leave('source' if sacrificed_source else 'sac_land')
        pools[owner][0]=0 if mode=='filter_unpayable_query' else p['generic']+(3 if mode=='value' else 0)
        pools[other][2]=len(twiddles)
        if mode in ('life_two','life_one_query'):
            life[owner]=2 if mode=='life_two' else 1;steps.append(step('set_life',player=owner,value=life[owner]))
        if mode in ('tapped','helper_tapped'):pools[other][0]+=1
        steps += fund(0,pools[0])+fund(1,pools[1])+[step('turn',player=other if mode=='opponent_turn' else owner,value=0),step('refresh')]
        if mode in ('tapped','helper_tapped'):
            target='helper' if mode=='helper_tapped' else 'source';steps += [step('choose_target',player=other,ref=target),step('activate',ref='tapper',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pools[other][0]-=1;entities['tapper']['tapped']=True;entities[target]['tapped']=True
        ready=len(steps)
        def pay_generic(player,amount):
            # Each tested cost is paid from a single actual pool color. No
            # arbitrary generic-color policy is presented as an Oracle rule.
            if amount:
                colors=[i for i,n in enumerate(pools[player]) if n]
                if len(colors)!=1 or pools[player][colors[0]]<amount:raise ValueError('single-color actual payment fixture required')
                pools[player][colors[0]]-=amount
        def payment_input(role):
            if role not in payment_inputs:return []
            chosen=payment_inputs[role]
            if owner!=0 or pools[owner][chosen]!=1 or sum(pools[owner])!=1:raise ValueError('measured pool click must spend the actual single colored mana')
            return [step('choose_mana',player=owner,color=chosen)]
        def activate():
            nonlocal activation_number
            activation_number+=1
            seq=[step('require_can_activate',ref='source',value=1)]
            costchoice=p['tap_other'] and ui['cost_creature_choices'][str(owner)]
            if costchoice and ui['cost_before_color']:seq.append(step('choose_target',player=owner,ref='helper'))
            menu_palette=set(palette)
            if presentation.get('combined_entry_palette'):menu_palette|={entry_color}|{s['fixed_color'] for s in [p]+p['siblings'] if s['output']=='fixed'}
            if presentation.get('creature_color_menu') and 'helper' in entities and entities['helper']['alive'] and not entities['helper']['tapped']:menu_palette=set(range(6))
            if len(menu_palette)>1 or ui['filter_menu'] and sum(pools[owner])>=1:seq.append(step('choose_color',player=owner,value=color))
            if costchoice and not ui['cost_before_color']:seq.append(step('choose_target',player=owner,ref='helper'))
            seq+=payment_input('source_activation_'+str(activation_number))
            seq.append(step('activate',ref='source',value=1));pay_generic(owner,p['generic']);life[owner]+=p['gain_life']-p['pay_life'];entities['source']['tapped']=True
            if p['tap_other']:entities['helper']['tapped']=True
            if palette:pools[owner][color]+=1
            return seq+check() # Mana and every coupled receipt must be immediate.
        def spend():
            seq=payment_input('spender')+[step('choose_target',player=owner,ref='enemy'),step('activate',ref='spender',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pay_generic(owner,1);entities['spender']['tapped']=True;entities['enemy']['tapped']=True
            return seq+check()
        deny=mode in ('tapped','entry_tapped','no_helper','helper_tapped') or mode=='fresh' and p['creature']
        query=mode.endswith('_query')
        cost_unpayable_with_free_c=p['tap_other'] and mode in ('no_helper','helper_tapped') and any(s['output']=='fixed' and s['fixed_color']==0 and not s['generic'] for s in p['siblings'])
        protected_cost_gap=mode=='helper_shroud' and presentation.get('shrouded_helper_query_only')
        if sacrificed_source:steps += [step('require_card',ref='source',property='in_play',value=0)]
        elif raw_policy:steps += [step('can_activate',ref='source',value=raw_policy['raw_value'])]
        elif cost_unpayable_with_free_c or protected_cost_gap:steps += [step('require_can_activate',ref='source',value=1)]
        elif deny:steps += [step('require_can_activate',ref='source',value=0)]
        elif query:steps += [step('require_can_activate',ref='source',value=1)]
        elif mode!='inactive':
            steps+=activate()
            if mode=='repeat_denied':steps.append(step('require_can_activate',ref='source',value=0))
            if mode=='repeat_untap':
                steps += twiddle(twiddles[0],'source')
                if p['tap_other']:steps += twiddle(twiddles[1],'helper')
                steps+=activate()
            if mode in ('spend','cleanup','untap_step','reach'):steps+=spend()
            if mode=='cleanup':steps += [step('cleanup'),step('refresh')]
            if mode=='untap_step':
                # The bounded phase API requires a genuinely prepared turn and
                # empty pools. The produced mana was spent above, not erased.
                steps += [step('advance_turn'),step('advance_turn'),step('untap_step',player=owner),step('upkeep_step',player=owner),step('turn',player=owner,value=0),step('refresh')]
                for e in entities.values():
                    if e['alive'] and e['player']==owner:e['tapped']=False
                steps+=activate()
            if mode=='reach':
                steps += twiddle(twiddles[0],'source')+[step('turn',player=other,value=0),step('begin_attackers'),step('attack',ref='flyer',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='flyer',value=1),step('advance_combat'),step('combat_damage'),step('resolve'),step('refresh'),step('end_combat'),step('assert_choices_consumed')]
                entities['flyer']['tapped']=True;entities['source']['damage']=1;leave('flyer')
        final=check();steps+=final
        if mode=='ready_snapshot':
            tail=copy.deepcopy(steps[ready:]);steps=steps[:ready]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('set_life',player=other,value=9),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        gaps=['recursive or modified land producibility','control changes','restricted/Snow mana spending provenance']
        if p['land']:gaps+=['legal land-play history: source is a normal battlefield land fixture, not a land spell']
        if p['output']=='own_types':gaps+=['actual zero-output action with an empty offered-color mask: query covered; report concrete presentation impediment before any shared hook']
        if raw_policy:gaps+=['Raw AI EVENT_CAN_ACTIVATE suppresses this nonproductive action; Oracle still permits T for zero output. AI action is not forced; the human zero-output action and productive AI counterparts remain separate real controls.']
        if cost_unpayable_with_free_c:gaps+=['Selected colored clause cannot pay its additional creature-tap cost in this fixture; global can_activate1 observes the distinct freeC sibling, not colored-mode legality.']
        if protected_cost_gap:gaps+=['Printed colored-mana clause with Shrouded own untapped creature: its non-target tap cost is Oracle-legal, but the actual protected-helper selection/action is unexecuted because the current source selector treats protection as targeting and no conditional mana-color/cost-selector receipt is available.'+(' Source-present/freeC availability query is unexecuted because the AI legally sacrificed Source to pay Safekeeper; no mana-rule denial is inferred.' if sacrificed_source else ' Global can_activate1 observes only a still-legal freeC sibling and does not prove colored-cost eligibility.')]
        if protected_cost_gap:gaps+=['Unexecuted selected clause for that protected-helper operation: '+oracle[name]['oracle_text'].splitlines()[p['ability']]]
        recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)
        if raw_policy:recipe['raw_ai_availability']=copy.deepcopy(raw_policy)
        if protected_cost_gap:recipe['oracle_cost_eligibility']=dict(shrouded_own_untapped_creature=True,is_target=False,executed_observation='global freeC availability only; protected colored-cost action unexecuted')
        if sacrificed_source:
            recipe['oracle_cost_eligibility']['executed_observation']='real paid Safekeeper Shroud; Source legally sacrificed, so Source-present/freeC query and protected colored-cost action unexecuted'
            recipe['observed_ai_sacrifice']=dict(ability_source='protector',sacrificed_ref='source',destination='owner_graveyard',surviving_land_ref='sac_land',input='legal_AI_autochoice')
        results.append(dict(name=f'PrintedTapManaActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=recipe))
    return results
