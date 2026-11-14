"""Oracle-derived permanent value abilities with real resource transactions."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

FAMILY='permanent_value_activation'
NUMBERS={'a':1,'an':1,'one':1,'two':2,'three':3,'four':4,'five':5,'six':6,'seven':7,'eight':8,'nine':9,'ten':10,'twenty':20}
LIBRARY=('Forest','Plains','Mountain','Island','Grizzly Bears','Suntail Hawk','Howling Mine','Lightning Bolt','Forest','Plains','Mountain','Island')
FIELDS=('name','oracle_id','layout','type_line','mana_cost','oracle_text','keywords','colors','color_identity','cmc','power','toughness','loyalty')
COUNTER_KINDS={'charge':'charge','-1/-1':'minus1','+1/+1':'plus1','elixir':'elixir','divinity':'divinity','quest':'quest','blood':'blood','gold':'gold','study':'study','knowledge':'knowledge'}

def pins():
    return json.loads((Path(__file__).parent/'data/permanent_value_activation_wave8_witnesses.json').read_text(encoding='utf8'))

def enters_tapped(record):
    return any(re.fullmatch(r'This (?:artifact|creature|enchantment|land|planeswalker) enters tapped\.',line)
               for line in record.get('oracle_text','').splitlines())

def number(word):
    if word=='X':return 'X'
    value=NUMBERS.get(word,int(word) if word.isdigit() else None)
    if value is None or not 0<=value<=20:raise ValueError('unsupported value quantity')
    return value

def mana(text):
    from recipes import cost_from_oracle
    text=text.replace('{X}','')
    if not text:return [0]*6
    return [0]*6 if text=='{0}' else cost_from_oracle(dict(mana_cost=text))

def parse_cost(text):
    result=dict(mana='',tap=False,pay_life=0,sacrifice=None,discard=0,x=False)
    if re.fullmatch(r'[+\u2212-]\d+|0',text):
        result['loyalty']=int(text.replace('\u2212','-'));return result
    for piece in text.split(', '):
        if re.fullmatch(r'(?:\{(?:\d+|[WUBRGCX])\})+',piece):
            if result['mana']:raise ValueError('duplicate mana cost')
            result['mana']=piece;result['x']='{X}' in piece
            if piece.count('{X}')>1:raise ValueError('multiple-X activation needs separate payment pilot')
        elif piece=='{T}':
            if result['tap']:raise ValueError('duplicate tap cost')
            result['tap']=True
        elif re.fullmatch(r'Pay \d+ life',piece):
            if result['pay_life']:raise ValueError('duplicate life cost')
            result['pay_life']=int(piece.split()[1])
        elif piece=='Discard your hand' or re.fullmatch(r'Discard (?:a|two|three) cards?',piece):
            if result['discard']:raise ValueError('duplicate discard cost')
            result['discard']='all' if piece=='Discard your hand' else number(piece.split()[1])
        elif piece.startswith('Sacrifice '):
            scope=piece[10:]
            if scope.startswith('this ') and scope[5:] in ('artifact','creature','enchantment','land','permanent'):scope='self'
            elif scope.startswith('a '):scope=scope[2:]
            elif scope.startswith('an '):scope=scope[3:]
            if scope not in ('self','creature','artifact','land','Forest','Beast','Bird','Cleric','Saproling','nontoken permanent'):
                raise ValueError('unpiloted sacrifice scope')
            if result['sacrifice']:raise ValueError('multiple sacrifice costs need a delta pilot')
            result['sacrifice']=scope
        elif (match:=re.fullmatch(r'Remove (a|one|two|three|four|five) (charge|-1/-1|\+1/\+1|elixir|divinity|quest|blood|gold|study|knowledge) counters? from this (?:artifact|creature|enchantment|permanent)',piece)):
            if 'counter' in result:raise ValueError('duplicate named counter cost')
            result['counter']=dict(kind=COUNTER_KINDS[match[2]],amount=number(match[1]))
        else:raise ValueError('unpiloted activation cost: '+piece)
    if not text or result['pay_life']>20:raise ValueError('nonempty bounded activation cost required')
    return result

def parse_effect(text):
    restriction='none'
    for tail,value in [(' Activate only during your turn.','own_turn'),(' Activate only as a sorcery.','sorcery'),(' Activate only once each turn.','once')]:
        if text.endswith(tail):text=text[:-len(tail)];restriction=value;break
    derived=[
        (r"You gain life equal to the sacrificed creature's (power|toughness)\.",lambda m:('gain_life',dict(kind='sacrifice_stat',stat=m[1]),'self')),
        (r"Target player mills cards equal to the sacrificed creature's power\.",lambda m:('mill',dict(kind='sacrifice_stat',stat='power'),'target')),
        (r'You gain (\d+) life for each (colorless )?creature you control\.',lambda m:('gain_life',dict(kind='permanent_count',scope='self',type='Creature',colorless=bool(m[2]),multiplier=number(m[1])),'self')),
        (r'You gain (\d+) life for each ([A-Z][a-z]+) on the battlefield\.',lambda m:('gain_life',dict(kind='permanent_count',scope='all',type=None,subtype=m[2],multiplier=number(m[1])),'self')),
        (r'You gain (\d+) life for each ([A-Z][a-z]+) you control\.',lambda m:('gain_life',dict(kind='permanent_count',scope='self',type=None,subtype=m[2],multiplier=number(m[1])),'self')),
        (r'Draw a card for each ([A-Z][a-z]+) you control\.',lambda m:('draw',dict(kind='permanent_count',scope='self',type=None,subtype=m[1],multiplier=1),'self')),
        (r'You gain life equal to the greatest power among creatures you control\.',lambda m:('gain_life',dict(kind='greatest_power',scope='self',type='Creature'),'self')),
        (r"Target player mills X cards, where X is the number of cards in that player's graveyard\.",lambda m:('mill',dict(kind='zone_count',scope='target',zone='graveyard'),'target')),
        (r'Target player mills X cards, where X is the number of creatures you control with defender\.',lambda m:('mill',dict(kind='permanent_count',scope='self',type='Creature',keyword='Defender',multiplier=1),'target')),
        (r'Draw a card for each tapped creature target player controls\.',lambda m:('draw',dict(kind='permanent_count',scope='target',type='Creature',tapped=True,multiplier=1),'self')),
    ]
    for pattern,build in derived:
        match=re.fullmatch(pattern,text)
        if match:
            op,amount,recipient=build(match)
            return [dict(op=op,amount=amount,recipient=recipient)],restriction
    # Ordering is retained explicitly, including discard before drawing.
    patterns=[
        (r'Draw (a|two|three|four|seven|X) cards?\.',lambda m:[('draw',number(m[1]),'self')]),
        (r'You gain (\d+|X) life\.',lambda m:[('gain_life',number(m[1]),'self')]),
        (r'You gain (\d+) life and draw (a|two|three) cards?\.',lambda m:[('gain_life',number(m[1]),'self'),('draw',number(m[2]),'self')]),
        (r'You draw (a|two|three) cards? and you lose (\d+) life\.',lambda m:[('draw',number(m[1]),'self'),('lose_life',number(m[2]),'self')]),
        (r'Discard (a|two) cards?, then draw (a|two) cards?\.',lambda m:[('discard',number(m[1]),'self'),('draw',number(m[2]),'self')]),
        (r'Target player mills (a|two|three|four|five|six|seven|ten|twenty|X) cards?\.',lambda m:[('mill',number(m[1]),'target')]),
        (r'Target player discards (a|two|three|four|X) cards?\.',lambda m:[('discard',number(m[1]),'target')]),
        (r'Target player gains (\d+) life\.',lambda m:[('gain_life',number(m[1]),'target')]),
        (r'Target player draws (a|two|three) cards?\.',lambda m:[('draw',number(m[1]),'target')]),
        (r'Each player draws (a|two|three) cards?\.',lambda m:[('draw',number(m[1]),'both')]),
        (r'Each player discards (a|two|three) cards?\.',lambda m:[('discard',number(m[1]),'both')]),
        (r'Target player loses (\d+) life and you gain (\d+) life\.',lambda m:[('lose_life',number(m[1]),'target'),('gain_life',number(m[2]),'self')]),
    ]
    for pattern,build in patterns:
        match=re.fullmatch(pattern,text)
        if match:return [dict(op=op,amount=amount,recipient=recipient) for op,amount,recipient in build(match)],restriction
    raise ValueError('unpiloted value effect: '+text)

def parse(record,index):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Token' in record.get('type_line','') or any(t in record.get('type_line','') for t in ('Aura','Equipment'))
            or not any(t in record.get('type_line','') for t in ('Creature','Artifact','Enchantment','Land','Planeswalker'))):
        raise ValueError('normal standalone permanent required')
    lines=record.get('oracle_text','').splitlines()
    if type(index) is not int or not 0<=index<len(lines) or ': ' not in lines[index]:raise ValueError('printed activation line required')
    cost,effect=lines[index].split(': ',1);cost=parse_cost(cost);derived_cost=None
    if effect=="Draw a card. X is the number of cards in an opponent's hand." and cost['mana']=='{X}':
        derived_cost=dict(kind='zone_count',scope='opponent',zone='hand');cost['x']=False
        effects,restriction=parse_effect('Draw a card.')
    elif effect=='Draw three cards. This ability costs {1} more to activate for each card in your hand.':
        derived_cost=dict(kind='zone_count',scope='self',zone='hand')
        effects,restriction=parse_effect('Draw three cards.')
    else:effects,restriction=parse_effect(effect)
    if any(e['amount']=='X' for e in effects) and not cost['x']:raise ValueError('X effect needs independently chosen printed X payment')
    creature='Creature' in record['type_line'];land='Land' in record['type_line']
    if creature and (any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness')) or int(record['toughness'])==0):
        raise ValueError('source needs surviving numeric printed creature stats')
    counter=None;walker='Planeswalker' in record['type_line']
    if walker:
        if 'loyalty' not in cost or not re.fullmatch(r'\d+',record.get('loyalty','')):raise ValueError('printed starting loyalty and loyalty cost required')
        initial=int(record['loyalty']);delta=cost['loyalty']
        if initial+delta<=0:raise ValueError('loyalty depletion/charging needs a separate actual-turn lifecycle pilot')
        counter=dict(kind='loyalty',initial=initial,delta=delta);restriction='sorcery'
    elif 'loyalty' in cost:raise ValueError('loyalty activation requires a planeswalker')
    elif 'counter' in cost:
        kind=cost['counter']['kind'];initial=None
        printed=next(key for key,value in COUNTER_KINDS.items() if value==kind)
        for line in lines:
            match=re.fullmatch(r'This (?:artifact|creature|enchantment) enters with (a|one|two|three|four|five) '+re.escape(printed)+r' counters? on it\.',line)
            if match:initial=number(match[1]);break
        if kind=='charge' and 'Whenever this artifact or another artifact enters, put a charge counter on this artifact.' in lines:initial=1
        if initial is None or initial<cost['counter']['amount']:raise ValueError('named counter cost needs a reviewed actual entry/trigger lifecycle')
        counter=dict(kind=kind,initial=initial,delta=-cost['counter']['amount'])
    return dict(family=FAMILY,ability=index,cost=cost,effects=effects,restriction=restriction,creature=creature,land=land,
                derived=derived_cost is not None or any(isinstance(e['amount'],dict) for e in effects),derived_cost=derived_cost,
                target_player=any(e['recipient']=='target' or isinstance(e['amount'],dict) and e['amount'].get('scope')=='target' for e in effects),
                casting_mana=None if land else mana(record['mana_cost']),activation_mana=mana(cost['mana']),
                gaps=[line for i,line in enumerate(lines) if i!=index],counter=counter,walker=walker,
                alternate_abilities=any(': ' in line for i,line in enumerate(lines) if i!=index),
                enters_tapped=enters_tapped(record),
                indestructible='Indestructible' in lines,conditional_creature=any("isn't a creature" in line for line in lines),
                haste=any('Haste' in [part.strip().capitalize() for part in line.split(',')] for line in lines))

def validate_unique_value_mode(record,index):
    """Auto-selection is safe only for one value ability and mana siblings."""
    mana_siblings=0
    for number,line in enumerate(record['oracle_text'].splitlines()):
        if number==index:continue
        if re.fullmatch(r'This (?:artifact|creature|enchantment|land|planeswalker) enters tapped\.',line):continue
        if ': ' not in line:raise ValueError('automatic value-mode intent requires only reviewed mana siblings and entry text')
        cost,effect=line.split(': ',1)
        if not re.fullmatch(r'Add \{[CWUBRG]\}(?: or \{[CWUBRG]\})?\.',effect):
            raise ValueError('automatic value-mode intent cannot choose between multiple nonmana abilities')
        paid=parse_cost(cost)
        if paid['sacrifice'] or paid['discard'] or paid['x'] or 'counter' in paid or 'loyalty' in paid:
            raise ValueError('unreviewed mana sibling cost for automatic value intent')
        mana_siblings+=1
    if not mana_siblings:raise ValueError('automatic value-mode intent requires a real mana sibling')

def spec(declaration,record):
    if declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2:
        raise ValueError('permanent value activation requires its family and integer version 2')
    p=declaration.get('params')
    ui_keys={'sacrifice_choices','present_ai_dialogs','resolution_dialogs','mode_dialog_minimum','silent_variants','fallback_color','activation_dialogs','sacrifice_first','mode_intent'}
    if 'activation_dialogs' in declaration:raise ValueError('permanent value mode annotations belong inside params')
    if not isinstance(p,dict) or 'ability' not in p or set(p)-({'ability'}|ui_keys) or declaration.get('oracle_text')!=(record or {}).get('oracle_text'):
        raise ValueError('explicit printed ability index and complete pinned Oracle text required')
    result=parse(record,p['ability'])
    selected=declaration.get('variants')
    if not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or len(set(selected))!=len(selected) or not set(selected)<=set(variants(result)):
        raise ValueError('explicit distinct supported value variants required')
    dialogs=p.get('activation_dialogs',{})
    if not isinstance(dialogs,dict) or any(k not in ('0','1') or type(v) is not int or not 0<=v<=16 for k,v in dialogs.items()):
        raise ValueError('reviewed per-controller activation mode choices required')
    sacrifice_choices=p.get('sacrifice_choices',{})
    if (not isinstance(sacrifice_choices,dict) or any(k not in ('0','1') or type(v) is not bool for k,v in sacrifice_choices.items())
            or sacrifice_choices and result['cost']['sacrifice'] is None):
        raise ValueError('sacrifice input presentation requires explicit controller booleans and a selectable sacrifice cost')
    if 'present_ai_dialogs' in p and type(p['present_ai_dialogs']) is not bool:raise ValueError('AI presentation annotation must be boolean')
    if 'sacrifice_first' in p and (type(p['sacrifice_first']) is not bool or not result['cost']['sacrifice']):raise ValueError('sacrifice ordering annotation requires a selectable cost and boolean')
    resolution_dialogs=p.get('resolution_dialogs',{})
    if not isinstance(resolution_dialogs,dict) or any(k not in ('0','1') or not isinstance(v,list) or any(type(choice) is not int or not 0<=choice<=16 for choice in v) for k,v in resolution_dialogs.items()):
        raise ValueError('resolution dialog inputs require explicit bounded controller lists')
    if 'mode_dialog_minimum' in p and (type(p['mode_dialog_minimum']) is not int or not 0<=p['mode_dialog_minimum']<=30):raise ValueError('mode dialog visibility threshold requires bounded explicit mana annotation')
    silent=p.get('silent_variants',[])
    if not isinstance(silent,list) or any(v not in variants(result) for v in silent) or len(set(silent))!=len(silent):raise ValueError('silent presentation variants must name supported cases exactly')
    fallback=p.get('fallback_color',{})
    if not isinstance(fallback,dict) or any(k not in ('0','1') or type(v) is not int or not 1<=v<=5 for k,v in fallback.items()) or fallback and 'mode_dialog_minimum' not in p:
        raise ValueError('fallback mana color inputs require a reviewed visibility threshold and controller colors')
    mode_intent=p.get('mode_intent')
    if mode_intent is not None:
        if (not isinstance(mode_intent,dict) or set(mode_intent)!={'mode_label','fallback_color','allow_auto_nonmana'}
                or not isinstance(mode_intent['mode_label'],str) or not mode_intent['mode_label'] or len(mode_intent['mode_label'].encode('utf8'))>127
                or '\n' in mode_intent['mode_label'] or '\r' in mode_intent['mode_label']
                or type(mode_intent['fallback_color']) is not int or not 0<=mode_intent['fallback_color']<=6
                or mode_intent['allow_auto_nonmana'] is not True or result['counter']
                or set(p)&{'mode_dialog_minimum','silent_variants','fallback_color','activation_dialogs'}):
            raise ValueError('source value intent requires an exact label/fallback and explicit unique automatic nonmana policy without numeric visibility forks')
        validate_unique_value_mode(record,p['ability'])
    expected=pins().get('source_interfaces',{}).get(record['name'],{}).get('abilities',{}).get(str(p['ability']),{})
    if {k:p[k] for k in ui_keys if k in p}!=expected.get('params',{}):
        raise ValueError('source activation input annotations differ from the independently reviewed interface')
    return result

def variants(p):
    if p.get('derived'):return derived_variants(p)
    if p.get('counter'):return counter_variants(p)
    modes=['value','payment','offturn','tapped','sick','empty_hand','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','cleanup']
    if not p['land']:modes+=['cast','fresh']
    if sum(p['activation_mana']):modes+=['insufficient_mana']
    if any(p['activation_mana'][1:]):modes+=['wrong_color']
    if p['cost']['tap']:modes+=['repeat_denied']
    if p['restriction']=='once' and p['cost']['sacrifice'] not in (None,'self'):modes+=['repeat_limit']
    if p['cost']['pay_life']:modes+=['insufficient_life','low_life']
    if p['cost']['x']:modes+=['x0','x1','x5']
    if any(e['recipient']=='target' for e in p['effects']):modes+=['target_self']
    if any(e['op']=='mill' for e in p['effects']):modes+=['short_library','empty_library']
    if p['cost']['sacrifice'] is None:modes+=['source_destroyed_pending']
    if p.get('alternate_abilities'):
        # The shared query sees any sibling ability. It cannot independently
        # prove an unavailable selected mode when another mode remains legal.
        modes=[v for v in modes if v not in ('insufficient_mana','wrong_color','insufficient_life','repeat_denied')
               and not (v in ('tapped','sick','cast','fresh') and p['cost']['tap'])
               and not (v=='empty_hand' and p['cost']['discard'])
               and not (v=='offturn' and p['restriction'] in ('own_turn','sorcery'))]
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]

def validate_witnesses(oracle,registry):
    for name,pin in pins()['witnesses'].items():
        if registry.get(name,{}).get('csvid')!=pin['csvid'] or any(oracle.get(name,{}).get(k)!=v for k,v in pin['oracle'].items()):
            raise ValueError('permanent value activation witness changed: '+name)

def sacrifice_eligible(record,scope):
    if scope=='self':return True
    typ=record.get('type_line','')
    if scope=='nontoken permanent':return 'Token' not in typ
    if scope in ('creature','artifact','land'):return scope.capitalize() in typ
    return scope in typ.partition(' — ')[2].split()

def expand(name,csvid,declaration,p,selected,registry,oracle):
    if p.get('derived'):return expand_derived(name,csvid,declaration,p,selected,registry,oracle)
    if p.get('counter'):return expand_counter(name,csvid,declaration,p,selected,registry,oracle)
    from recipes import card,step
    validate_witnesses(oracle,registry)
    result=[];witness_names=set(pins()['witnesses'])
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);other=1-owner
        cost=p['cost'];x={'x0':0,'x1':1,'x5':5}.get(mode,3)
        target=owner if mode=='target_self' else other
        libs=[list(LIBRARY),list(LIBRARY)];hands=[['Plains'],['Plains']];graves=[['Lightning Bolt'],['Lightning Bolt']];exiles=[['Island'],['Island']]
        if mode in ('empty_hand','bare_zones'):hands=[[],[]]
        if mode=='bare_zones':graves=[[],[]];exiles=[[],[]]
        if mode=='short_library':libs[target]=libs[target][:2]
        if mode=='empty_library':libs[target]=[]
        entities={};setup=[];actions=[];life=[20,20]
        casting=mode in ('cast','fresh')
        source_state=['tapped'] if mode=='tapped' else ['sick'] if mode=='sick' else None
        def add(nm,ref,who,zone='battlefield',state=None):
            if nm!=name and nm not in witness_names:raise ValueError('unreviewed permanent value activation witness: '+nm)
            item=card(nm,ref,who,zone)
            token=oracle[nm].get('type_line','').startswith('Token ')
            # Every established battlefield entity has its modeled ready state,
            # including opposing controls whose real entry event also runs.
            if zone=='battlefield' and enters_tapped(oracle[nm]) and not set(state or [])&{'tapped','untapped'}:
                state=list(state or [])+['untapped']
            if state or token:item['state']=list(state or [])+(['token'] if token else [])
            setup.append(item);entities[ref]=dict(name=nm,player=who,alive=zone=='battlefield',tapped=bool(state and 'tapped' in state),token=token)
            if zone=='hand':hands[who].append(nm)
        # Zone witnesses are copied into the fixture exactly once, separately
        # from later source casts and cost movement modeled below.
        for who in (0,1):
            for zone,names in (('hand',hands[who]),('library',libs[who]),('graveyard',graves[who]),('exile',exiles[who])):
                setup.extend(card(nm,f'{zone}_{who}_{i}',who,zone) for i,nm in enumerate(names))
        add(name,'source',owner,'hand' if casting else 'battlefield',source_state)
        scope=cost['sacrifice'];victim=None
        if scope:
            if scope=='self' or sacrifice_eligible(oracle[name],scope):victim='source'
            else:
                witness={'creature':'Ornithopter','artifact':'Howling Mine','land':'Forest','Forest':'Forest','Beast':'Rumbling Baloth','Bird':'Suntail Hawk','Cleric':'Devoted Caretaker','Saproling':'Saproling','nontoken permanent':'Howling Mine'}[scope]
                add(witness,'victim',owner,state=['tapped'] if 'Land' in oracle[witness]['type_line'] else None);victim='victim'
                if mode=='repeat_limit':add(witness,'spare',owner,state=['tapped'] if 'Land' in oracle[witness]['type_line'] else None)
            # Contrary controls are never eligible resources for this cost.
            control='Hill Giant' if scope in ('artifact','land','Forest','nontoken permanent') else 'Howling Mine'
            if scope!='nontoken permanent':add(control,'control',owner)
            enemy=entities[victim]['name'];add(enemy,'enemy_eligible',other,state=['tapped'] if 'Land' in oracle[enemy]['type_line'] else None)
        else:add('Hill Giant','control',owner);add('Grizzly Bears','enemy_control',other)
        activation=list(p['activation_mana']);activation[0]+=x*int(cost['x'])
        pools=[[30,10,10,10,10,10] for _ in (0,1)]
        if mode=='payment':pools[owner]=list(activation)
        if mode=='insufficient_mana':pools[owner]=[0]*6
        if mode=='wrong_color':
            pools[owner]=list(activation);color=next(i for i,n in enumerate(activation) if i and n);pools[owner][0]+=pools[owner][color];pools[owner][color]=0
        totals=[sum(pool) for pool in pools]
        if mode in ('insufficient_life','low_life'):life[owner]=cost['pay_life']-1 if mode=='insufficient_life' else cost['pay_life']+1
        current_turn=other if mode=='offturn' else owner
        actions.append(step('turn',player=current_turn,value=0))
        for who in (0,1):
            actions.extend(step('set_mana',player=who,color=color,value=value) for color,value in enumerate(pools[who]))
            actions.append(step('set_life',player=who,value=life[who]))
        actions.append(step('discard_policy',value=1))
        if declaration['params'].get('present_ai_dialogs'):actions.append(step('present_ai_dialogs',value=1))
        if casting:
            # Source casting uses a separate real payment and then restores an
            # explicit activation budget, avoiding hidden source-cost receipts.
            actions.extend([step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
            actions.append(step('assert_mana_total',player=owner,value=totals[owner]-sum(p['casting_mana'])))
            hands[owner].remove(name);entities['source']['alive']=True
            if p.get('enters_tapped'):entities['source']['tapped']=True
            actions.extend(step('set_mana',player=who,color=color,value=value) for who in (0,1) for color,value in enumerate(pools[who]))
        actions.append(step('refresh'))
        legal=(mode not in ('insufficient_mana','wrong_color','insufficient_life') and not (mode=='tapped' and cost['tap'])
               and not ((mode=='sick' or casting) and p['creature'] and cost['tap'] and not p.get('haste'))
               and not (casting and p.get('enters_tapped') and cost['tap'])
               and not (mode=='offturn' and p['restriction'] in ('own_turn','sorcery'))
               and not (type(cost['discard']) is int and len(hands[owner])<cost['discard']))
        def leave(ref,zone):
            e=entities[ref];e['alive']=False
            if not e['token']:(graves if zone=='graveyard' else hands)[e['player']].append(e['name'])
        def receipts(stack=0):
            seq=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for ref,e in entities.items():
                if mode=='repeat_limit' and ref in ('victim','spare'):continue
                seq.append(step('require_card',ref=ref,property='in_play',value=int(e['alive'])))
                if e['alive'] and 'Creature' in oracle[e['name']]['type_line'] and not (ref=='source' and p.get('conditional_creature')):
                    seq.extend(step('require_card',ref=ref,property=prop,value=int(oracle[e['name']][prop])) for prop in ('power','toughness'))
                if e['alive']:seq.append(step('require_card',ref=ref,property='tapped',value=int(e['tapped'])))
                if e['alive'] and e['token']:seq.append(step('assert_matching_permanents',player=e['player'],index=registry[e['name']]['csvid'],property='token',value=1))
            for who in (0,1):
                seq.extend([step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=totals[who]),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))])
                for zone,names in (('hand',hands[who]),('graveyard',graves[who]),('library',libs[who]),('exile',exiles[who])):
                    seq.append(step('assert_zone',player=who,zone=zone,value=len(names)))
                    if zone=='library':seq.extend(step('assert_zone_card',player=who,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names))
                    else:seq.extend(step('assert_zone_multiplicity',player=who,zone=zone,index=registry[nm]['csvid'],value=count) for nm,count in Counter(names).items())
            if mode=='repeat_limit':seq.append(step('assert_zone_multiplicity',player=owner,zone='battlefield',index=registry[entities[victim]['name']]['csvid'],value=1 if not entities[victim]['alive'] else 2))
            return seq
        ready=len(actions)
        if not legal:
            unavailable_discard_mode=(p['alternate_abilities'] and mode in ('empty_hand','bare_zones')
                                      and type(cost['discard']) is int and len(hands[owner])<cost['discard'])
            if unavailable_discard_mode:
                # A sibling remains legal; select this actually disabled menu
                # option to test its missing cost without activating a sibling.
                choice=declaration['params'].get('activation_dialogs',{}).get(str(owner))
                if choice is None or not declaration['params'].get('present_ai_dialogs'):
                    raise ValueError('unavailable discard mode needs its reviewed displayed menu option')
                actions.extend([step('require_can_activate',ref='source',value=1),step('choose_dialog',player=owner,value=choice)])
            else:actions.append(step('require_can_activate',ref='source',value=0))
            # The cast profile checks paid entry and its immediate permission;
            # the fresh profile separately attempts the denied activation.
            if mode!='cast':actions.append(step('activate',ref='source',value=0))
        else:
            actions.append(step('require_can_activate',ref='source',value=1))
            mode_intent=declaration['params'].get('mode_intent')
            if mode_intent:
                actions.append(step('choose_activation_mode',ref='source',player=owner,**mode_intent))
            elif (str(owner) in declaration['params'].get('activation_dialogs',{}) and variant not in declaration['params'].get('silent_variants',[])
                    and totals[owner]>=declaration['params'].get('mode_dialog_minimum',0)):
                actions.append(step('choose_dialog',player=owner,value=declaration['params']['activation_dialogs'][str(owner)]))
            elif str(owner) in declaration['params'].get('fallback_color',{}) and totals[owner]<declaration['params'].get('mode_dialog_minimum',0):
                actions.append(step('choose_color',player=owner,value=declaration['params']['fallback_color'][str(owner)]))
            if cost['x']:actions.append(step('choose_x',player=owner,value=x))
            sacrifice_input=(step('choose_target',player=owner,ref=victim) if scope and declaration['params'].get('sacrifice_choices',{}).get(str(owner),scope!='self' and owner==0) else None)
            if sacrifice_input and declaration['params'].get('sacrifice_first'):actions.append(sacrifice_input)
            if any(e['recipient']=='target' for e in p['effects']):actions.append(step('choose_player',player=owner,value=target))
            if sacrifice_input and not declaration['params'].get('sacrifice_first'):actions.append(sacrifice_input)
            actions.append(step('activate',ref='source',value=1))
            if mode_intent:actions.append(step('require_activation_mode',value=1))
            actions.extend([step('assert_stack',value=1),step('assert_choices_consumed')])
            totals[owner]-=sum(activation);life[owner]-=cost['pay_life']
            if cost['tap'] and not (victim=='source'):entities['source']['tapped']=True
            if victim:leave(victim,'graveyard')
            if cost['discard']:
                n=len(hands[owner]) if cost['discard']=='all' else min(cost['discard'],len(hands[owner]));graves[owner].extend(hands[owner][:n]);del hands[owner][:n]
            actions.extend(receipts(1));pending=len(actions)
            if mode=='source_destroyed_pending':
                actions.extend([step('destroy',ref='source'),step('refresh')])
                if not p.get('indestructible'):leave('source','graveyard')
            actions.extend(step('choose_dialog',player=owner,value=choice) for choice in declaration['params'].get('resolution_dialogs',{}).get(str(owner),[]))
            actions.extend([step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
            for effect in p['effects']:
                count=x if effect['amount']=='X' else effect['amount']
                players=(0,1) if effect['recipient']=='both' else (target,) if effect['recipient']=='target' else (owner,)
                for who in players:
                    op=effect['op']
                    if op=='draw':
                        if len(libs[who])<count:raise ValueError('draw beyond empty library needs a game-loss observer pilot')
                        hands[who].extend(libs[who][:count]);del libs[who][:count]
                    elif op=='mill':n=min(count,len(libs[who]));graves[who].extend(libs[who][:n]);del libs[who][:n]
                    elif op=='discard':n=min(count,len(hands[who]));graves[who].extend(hands[who][:n]);del hands[who][:n]
                    elif op=='gain_life':life[who]+=count
                    elif op=='lose_life':life[who]-=count
            if mode=='repeat_denied':actions.append(step('require_can_activate',ref='source',value=0))
            if mode=='repeat_limit':actions.append(step('require_can_activate',ref='source',value=0))
            if mode=='cleanup':actions.extend([step('cleanup'),step('refresh')])
        actions.extend(receipts())
        if mode=='payment' and legal:actions.extend(step('assert_mana',player=owner,color=color,value=0) for color in range(6))
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending;prefix=actions[:cut];replay=actions[cut:]
            actions=prefix+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':actions.extend([step('capture'),step('record_state'),step('set_life',player=other,value=7),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
        result.append(dict(name=f'PermanentValueActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_components=p['gaps']+['replacement/prevention effects','multiplayer','choice among multiple equally eligible sacrificial resources'])))
    return result


def counter_variants(p):
    modes=['value','payment','offturn','empty_hand','bare_zones','ready_snapshot','pending_snapshot',
           'resolved_snapshot','cleanup','repeat_denied','source_destroyed_pending']
    if p['cost']['tap']:modes+=['tapped']
    if sum(p['activation_mana']):modes+=['insufficient_mana']
    if any(p['activation_mana'][1:]):modes+=['wrong_color']
    if any(e['recipient']=='target' for e in p['effects']):modes+=['target_self']
    if any(e['op']=='mill' for e in p['effects']):modes+=['short_library','empty_library']
    if not p['walker'] and p['counter']['initial']+p['counter']['delta']==0:modes+=['spent_counter']
    return [mode+'_'+str(owner) for owner in (0,1) for mode in modes]


def derived_variants(p):
    expression=p['derived_cost'] or p['effects'][0]['amount'];kind=expression['kind']
    sacrifices_source=(p['cost']['sacrifice']=='self' or kind=='sacrifice_stat' and p['creature'] and p['cost']['sacrifice']=='creature')
    modes=['value','payment','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot',
           'cleanup','source_destroyed_pending','offturn']
    if p['cost']['tap']:modes+=['tapped','repeat_denied']
    if p['creature']:modes+=['sick','fresh']
    if p['walker']:modes+=['repeat_denied']
    if any(e['recipient']=='target' for e in p['effects']):modes+=['target_self','short_library','empty_library']
    elif p['target_player']:modes+=['target_self']
    if kind=='sacrifice_stat':
        modes+=['stat_pumped']
        if not sacrifices_source and not p['alternate_abilities']:modes+=['no_victim']
        if expression['stat']=='power':modes+=['zero_stat']
    elif kind in ('permanent_count','greatest_power'):modes+=['metric_many','no_other_members','metric_removed','zero_after_source_removal']
    elif kind=='zone_count' and expression['zone']=='graveyard':modes+=['metric_many','metric_removed']
    if kind=='greatest_power':modes+=['stat_pumped','stat_pumped_cleanup']
    if p['derived_cost']:
        modes+=(['empty_hand_cost','three_hand_cost','short_cost','cast'] if p['derived_cost']['scope']=='self'
                else ['cost_zero','cost_many','insufficient_mana'])
    elif sum(p['activation_mana']) and not p['alternate_abilities']:modes+=['insufficient_mana']
    if expression.get('keyword'):modes+=['one_short_payment','wrong_color']
    if expression.get('tapped'):modes+=['real_tap_before']
    if sacrifices_source:modes=[v for v in modes if v not in ('source_destroyed_pending','zero_after_source_removal')]
    return [mode+'_'+str(owner) for owner in (0,1) for mode in dict.fromkeys(modes)]


def derived_amount(expression,owner,target,entities,zones,saved):
    """Evaluate only typed Oracle expressions against the modeled live state."""
    if type(expression) is int:
        if not 0<=expression<=64:raise ValueError('derived fixture amount outside bounded model')
        return expression
    if not isinstance(expression,dict):raise ValueError('typed derived value expression required')
    kind=expression['kind']
    if kind=='sacrifice_stat':value=saved[expression['stat']]
    elif kind=='zone_count':
        who={'self':owner,'opponent':1-owner,'target':target}[expression['scope']]
        value=len(zones[expression['zone']][who])
    elif kind in ('permanent_count','greatest_power'):
        if expression['scope'] not in ('self','target','all'):raise ValueError('unreviewed permanent amount scope')
        matches=[e for e in entities.values() if e['alive'] and (expression['type'] is None or expression['type'] in e['record']['type_line'])
                 and (expression['scope']=='all' or e['player']==(target if expression['scope']=='target' else owner))
                 and (not expression.get('subtype') or expression['subtype'] in e['record']['type_line'].partition(' — ')[2].split())
                 and (not expression.get('colorless') or not e['record']['colors'])
                 and (not expression.get('keyword') or expression['keyword'] in e['record']['keywords'])
                 and ('tapped' not in expression or e['tapped']==expression['tapped'])]
        value=(len(matches)*expression.get('multiplier',1) if kind=='permanent_count'
               else max([0]+[e['power'] for e in matches]))
    else:raise ValueError('unreviewed derived value expression')
    if type(value) is not int or not 0<=value<=64:raise ValueError('derived fixture amount outside bounded model')
    return value


def expand_derived(name,csvid,declaration,p,selected,registry,oracle):
    """Keep announcement costs/LKI separate from resolution-time quantities."""
    from recipes import card,step
    validate_witnesses(oracle,registry);witnesses=set(pins()['witnesses']);result=[]
    expression=p['derived_cost'] or p['effects'][0]['amount'];kind=expression['kind'];cost=p['cost']
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);other=1-owner;target=owner if mode=='target_self' else other
        zones=dict(hand=[['Plains'],['Plains']],library=[list(LIBRARY),list(LIBRARY)],graveyard=[['Lightning Bolt'],['Lightning Bolt']],exile=[['Island'],['Island']])
        if mode=='bare_zones':zones['hand']=[[],[]];zones['graveyard']=[[],[]];zones['exile']=[[],[]]
        if p['derived_cost']:
            if p['derived_cost']['scope']=='self':
                zones['hand'][owner]=[] if mode in ('empty_hand_cost','bare_zones') else ['Plains']*3 if mode in ('three_hand_cost','short_cost') else ['Plains']
                if mode!='bare_zones':zones['hand'][other]=['Plains']*2
            else:zones['hand'][other]=[] if mode in ('cost_zero','bare_zones') else ['Plains']*3 if mode in ('cost_many','insufficient_mana') else ['Plains']
        if kind=='zone_count' and expression['zone']=='graveyard' and mode in ('metric_many','short_library'):zones['graveyard'][target]=['Lightning Bolt','Forest','Island']
        if mode=='short_library':zones['library'][target]=zones['library'][target][:2]
        if mode=='empty_library':zones['library'][target]=[]
        setup=[];actions=[];entities={};life=[20,20];saved={};counter=p['counter']['initial'] if p['counter'] else None
        for zone,players in zones.items():
            for who,names in enumerate(players):setup.extend(card(nm,f'{zone}_{who}_{i}',who,zone) for i,nm in enumerate(names))
        def add(nm,ref,who,zone='battlefield',tapped=False):
            if nm!=name and nm not in witnesses:raise ValueError('unpinned derived value witness: '+nm)
            rec=oracle[nm];item=card(nm,ref,who,zone)
            if zone=='battlefield':item['state']=['tapped'] if tapped else ['untapped']
            setup.append(item)
            entities[ref]=dict(name=nm,record=rec,player=who,alive=zone=='battlefield',tapped=tapped,
                               power=int(rec.get('power','0')),toughness=int(rec.get('toughness','0')))
            if zone=='hand':zones['hand'][who].append(nm)
        casting=p['walker'] or mode in ('fresh','cast')
        add(name,'source',owner,'hand' if casting else 'battlefield',mode=='tapped')
        if mode=='sick':next(s for s in setup if s['ref']=='source')['state'].append('sick')
        victim=None
        if kind=='sacrifice_stat':
            if p['creature'] and cost['sacrifice']=='creature':victim='source'
            elif mode!='no_victim':
                nm='Ornithopter' if expression['stat']=='toughness' or mode in ('zero_stat','stat_pumped') else 'Hill Giant'
                add(nm,'victim',owner);victim='victim'
            add('Howling Mine','control',owner);add('Ornithopter','enemy_control',other)
        elif kind in ('permanent_count','greatest_power'):
            minimal=mode in ('no_other_members','zero_after_source_removal')
            if expression.get('keyword'):
                member_name=pins().get('derived_keyword_witnesses',{}).get(expression['keyword'])
                if member_name not in oracle or expression['keyword'] not in oracle[member_name]['keywords']:
                    raise ValueError('derived keyword count needs an independently pinned printed keyword witness')
                if not minimal:add(member_name,'member',owner)
                add(member_name,'enemy_member',other);add('Ornithopter','control',owner)
            elif expression.get('tapped'):
                add('Hill Giant','member',other,tapped=not minimal)
                add('Grizzly Bears','untapped_member',other)
                add('Rumbling Baloth','own_tapped1',owner,tapped=True);add('Suntail Hawk','own_tapped2',owner,tapped=True)
                add('Howling Mine','tapped_artifact',other,tapped=True)
                if mode=='real_tap_before':add('Icy Manipulator','metric_tapper',owner)
            elif expression.get('subtype'):
                member_name=name
                if expression['subtype'] not in oracle[name]['type_line'].partition(' — ')[2].split():
                    member_name=pins().get('derived_subtype_witnesses',{}).get(expression['subtype'])
                if member_name not in oracle or expression['subtype'] not in oracle[member_name]['type_line'].partition(' — ')[2].split():
                    raise ValueError('derived count needs a real independently pinned subtype witness')
                if not minimal:add(member_name,'member',owner);add(member_name,'enemy_member',other)
                add('Hill Giant','control',owner)
            elif expression.get('colorless'):
                if not minimal:add('Ornithopter','member',owner)
                add('Hill Giant','control',owner);add('Ornithopter','enemy_member',other)
            else:
                if not minimal:add('Hill Giant','member',owner);add('Grizzly Bears','second_member',owner)
                add('Rumbling Baloth','enemy_member',other)
            add('Howling Mine','artifact_control',owner)
            if mode=='metric_many':
                nm=member_name if expression.get('subtype') or expression.get('keyword') else 'Ornithopter' if expression.get('colorless') else 'Rumbling Baloth'
                who=target if expression['scope']=='target' else owner
                add(nm,'extra_member',who,tapped=bool(expression.get('tapped')));add(nm,'extra_member2',who,tapped=bool(expression.get('tapped')))
        else:
            add('Hill Giant','control',owner);add('Grizzly Bears','member',target)
        if cost['sacrifice']=='self':victim='source'
        pumped=mode in ('stat_pumped','stat_pumped_cleanup');pump_ref=victim if kind=='sacrifice_stat' else 'member'
        if pumped:
            if 'Giant Growth' not in witnesses:raise ValueError('actual stat change needs pinned Giant Growth')
            setup.append(card('Giant Growth','stat_spell',owner,'hand'));zones['hand'][owner].append('Giant Growth')
        def mana_pool(who,pool):actions.extend(step('set_mana',player=who,color=color,value=value) for color,value in enumerate(pool))
        if casting:
            actions.append(step('turn',player=owner,value=0));mana_pool(owner,p['casting_mana'])
            actions.extend([step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed'),step('assert_mana_total',player=owner,value=0)])
            zones['hand'][owner].remove(name);entities['source']['alive']=True
            if counter is not None:actions.append(step('assert_counter',ref='source',counter_kind=p['counter']['kind'],value=counter))
        activation=list(p['activation_mana'])
        if p['derived_cost']:activation[0]+=derived_amount(p['derived_cost'],owner,target,entities,zones,saved)
        pools=[[30,10,10,10,10,10] for _ in (0,1)]
        if mode in ('payment','empty_hand_cost','three_hand_cost'):pools[owner]=list(activation)
        if mode=='insufficient_mana':pools[owner]=[max(0,sum(activation)-1),0,0,0,0,0]
        if mode in ('short_cost','one_short_payment'):
            pools[owner]=list(activation);index=next(i for i,n in enumerate(activation) if n);pools[owner][index]-=1
        if mode=='wrong_color':
            pools[owner]=list(activation);color=next(i for i,n in enumerate(activation) if i and n);pools[owner][0]+=pools[owner][color];pools[owner][color]=0
        totals=[sum(pool) for pool in pools]
        actions.append(step('turn',player=other if mode=='offturn' else owner,value=0))
        for who in (0,1):mana_pool(who,pools[who]);actions.append(step('set_life',player=who,value=20))
        actions.extend([step('discard_policy',value=1),step('refresh')])
        if declaration['params'].get('present_ai_dialogs'):actions.append(step('present_ai_dialogs',value=1))
        if mode=='real_tap_before':
            actions.extend([step('require_card',ref='untapped_member',property='tapped',value=0),
                            step('require_can_activate',ref='metric_tapper',value=1),step('choose_target',player=owner,ref='untapped_member'),
                            step('activate',ref='metric_tapper',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),
                            step('assert_stack',value=0),step('assert_choices_consumed'),step('require_card',ref='untapped_member',property='tapped',value=1)])
            entities['untapped_member']['tapped']=True;entities['metric_tapper']['tapped']=True;totals[owner]-=1
        if pumped:
            e=entities[pump_ref]
            actions.extend(step('require_card',ref=pump_ref,property=prop,value=e[prop]) for prop in ('power','toughness'))
            actions.extend([step('choose_target',player=owner,ref=pump_ref),step('cast',ref='stat_spell',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
            zones['hand'][owner].remove('Giant Growth');zones['graveyard'][owner].append('Giant Growth');totals[owner]-=sum(mana(oracle['Giant Growth']['mana_cost']))
            for prop in ('power','toughness'):e[prop]+=3
        def leave(ref):
            e=entities[ref]
            if e['alive']:e['alive']=False;zones['graveyard'][e['player']].append(e['name'])
        def receipts(stack=0):
            seq=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for ref,e in entities.items():
                seq.append(step('require_card',ref=ref,property='in_play',value=int(e['alive'])))
                if e['alive']:
                    seq.append(step('require_card',ref=ref,property='tapped',value=int(e['tapped'])))
                    if 'Creature' in e['record']['type_line']:seq.extend(step('require_card',ref=ref,property=prop,value=e[prop]) for prop in ('power','toughness'))
            if counter is not None and entities['source']['alive']:seq.append(step('assert_counter',ref='source',counter_kind=p['counter']['kind'],value=counter))
            for who in (0,1):
                seq.extend([step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=totals[who]),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))])
                for zone,players in zones.items():
                    names=players[who];seq.append(step('assert_zone',player=who,zone=zone,value=len(names)))
                    if zone=='library':seq.extend(step('assert_zone_card',player=who,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names))
                    else:seq.extend(step('assert_zone_multiplicity',player=who,zone=zone,index=registry[nm]['csvid'],value=n) for nm,n in Counter(names).items())
            return seq
        actions.extend(receipts())
        legal=(mode not in ('insufficient_mana','short_cost','one_short_payment','wrong_color','no_victim') and not (mode=='tapped' and cost['tap'])
               and not (mode in ('sick','fresh') and p['creature'] and cost['tap'])
               and not (mode=='offturn' and p['restriction'] in ('own_turn','sorcery')))
        ready=len(actions);pending=None
        if not legal:actions.extend([step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)])
        else:
            actions.append(step('require_can_activate',ref='source',value=1))
            if victim and kind=='sacrifice_stat':
                saved={prop:entities[victim][prop] for prop in ('power','toughness')}
                actions.extend(step('require_card',ref=victim,property=prop,value=saved[prop]) for prop in ('power','toughness'))
            ui=declaration['params'];intent=ui.get('mode_intent')
            if intent:actions.append(step('choose_activation_mode',ref='source',player=owner,**intent))
            elif str(owner) in ui.get('activation_dialogs',{}):actions.append(step('choose_dialog',player=owner,value=ui['activation_dialogs'][str(owner)]))
            sacrifice=(step('choose_target',player=owner,ref=victim) if victim and cost['sacrifice']!='self' and ui.get('sacrifice_choices',{}).get(str(owner),owner==0) else None)
            if sacrifice and ui.get('sacrifice_first'):actions.append(sacrifice)
            if p['target_player']:actions.append(step('choose_player',player=owner,value=target))
            if sacrifice and not ui.get('sacrifice_first'):actions.append(sacrifice)
            actions.append(step('activate',ref='source',value=1))
            if intent:actions.append(step('require_activation_mode',value=1))
            totals[owner]-=sum(activation)
            if cost['tap']:entities['source']['tapped']=True
            if counter is not None:
                counter+=p['counter']['delta'];actions.append(step('assert_counter',ref='source',counter_kind=p['counter']['kind'],value=counter))
            if victim:leave(victim)
            actions.extend(receipts(1));pending=len(actions)
            removal='source' if mode in ('source_destroyed_pending','zero_after_source_removal') else 'member' if mode=='metric_removed' else None
            if removal:
                actions.extend([step('destroy',ref=removal),step('refresh')]);leave(removal);actions.extend(receipts(1))
            actions.extend([step('resolve'),step('refresh'),step('assert_stack',value=0),step('assert_choices_consumed')])
            for effect in p['effects']:
                count=derived_amount(effect['amount'],owner,target,entities,zones,saved)
                who=target if effect['recipient']=='target' else owner
                if effect['op']=='gain_life':life[who]+=count
                elif effect['op']=='draw':
                    if count>len(zones['library'][who]):raise ValueError('derived draw exceeds explicit library')
                    zones['hand'][who].extend(zones['library'][who][:count]);del zones['library'][who][:count]
                elif effect['op']=='mill':
                    count=min(count,len(zones['library'][who]));zones['graveyard'][who].extend(zones['library'][who][:count]);del zones['library'][who][:count]
                else:raise ValueError('unreviewed derived effect transaction')
            if mode=='repeat_denied':actions.append(step('require_can_activate',ref='source',value=0))
            if mode in ('cleanup','stat_pumped_cleanup'):
                actions.extend([step('cleanup'),step('refresh')])
                if pumped:
                    for prop in ('power','toughness'):entities[pump_ref][prop]-=3
        actions.extend(receipts())
        if mode in ('payment','empty_hand_cost','three_hand_cost') and legal:actions.extend(step('assert_mana',player=owner,color=color,value=0) for color in range(6))
        if mode in ('ready_snapshot','pending_snapshot'):
            cut=ready if mode=='ready_snapshot' else pending;prefix=actions[:cut];replay=actions[cut:]
            actions=prefix+[step('capture')]+copy.deepcopy(replay)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(replay)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':actions.extend([step('capture'),step('record_state'),step('set_life',player=other,value=7),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')])
        result.append(dict(name=f'PermanentValueActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_components=p['gaps']+['replacement/prevention effects','multiplayer','negative creature power','type/color-changing effects']
                                        +(['dynamic keyword gain/loss or suppression'] if expression.get('keyword') else [])
                                        +(['selected-mode denial while a sibling remains legal'] if p['alternate_abilities'] else []))))
    return result

def expand_counter(name,csvid,declaration,p,selected,registry,oracle):
    """Acquire counters by casting, then observe real paid costs and replay.

    The ordinary zone/payment/effect model remains shared. Additional counter
    receipts are inserted at actual engine action boundaries, never by setting
    counter state. Twiddle and Icy Manipulator drive real untapping/tapping.
    """
    from recipes import card,step
    result=[]
    base=copy.deepcopy(p);counter=base.pop('counter');base['counter']=None
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);other=1-owner
        case=expand(name,csvid,declaration,base,[variant],registry,oracle)[0]
        source=next(item for item in case['setup'] if item['ref']=='source')
        source['zone']='hand';source.pop('state',None)
        prefix=[step('turn',player=owner,value=0)]
        prefix += [step('set_mana',player=owner,color=color,value=value) for color,value in enumerate(p['casting_mana'])]
        prefix += [step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),
                   step('assert_stack',value=0),step('assert_choices_consumed'),step('require_card',ref='source',property='in_play',value=1),
                   step('assert_mana_total',player=owner,value=0),step('assert_counter',ref='source',counter_kind=counter['kind'],value=counter['initial'])]
        if counter['kind'] in ('plus1','minus1'):
            sign=1 if counter['kind']=='plus1' else -1
            prefix.extend(step('require_card',ref='source',property=prop,value=int(oracle[name][prop])+sign*counter['initial']) for prop in ('power','toughness'))
        tapped=mode=='tapped' and p['cost']['tap']
        if tapped:
            case['setup'].append(card('Icy Manipulator','counter_tapper',other,'battlefield'))
            prefix += [step('require_can_activate',ref='counter_tapper',value=1),step('choose_target',player=other,ref='source'),
                       step('activate',ref='counter_tapper',value=1),step('resolve'),step('refresh'),step('assert_stack',value=0),
                       step('assert_choices_consumed'),step('require_card',ref='source',property='tapped',value=1),
                       step('require_card',ref='counter_tapper',property='tapped',value=1),step('assert_mana_total',player=other,value=59)]
        spent=mode=='spent_counter' and p['cost']['tap']
        steps=case['steps']
        if spent:
            case['setup'].append(card('Twiddle','counter_untapper',owner,'hand'))
            resolution=next(i for i,s in enumerate(steps) if s['op']=='resolve')
            cut=resolution+4
            steps=steps[:cut]+[step('choose_target',player=owner,ref='source'),step('cast',ref='counter_untapper',value=1),
                              step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed'),step('refresh'),
                              step('require_card',ref='source',property='tapped',value=0),step('require_can_activate',ref='source',value=0)]+steps[cut:]
        transformed=[];count=counter['initial'];alive=True;captured=None;untapper_spent=False
        for original in steps:
            s=copy.deepcopy(original);op=s['op']
            if op=='capture':captured=(count,alive)
            if op=='restore':count,alive=captured
            if op=='cast' and s.get('ref')=='counter_untapper':untapper_spent=True
            if op=='assert_zone' and s.get('zone')=='battlefield' and tapped and s['player']==other:s['value']+=1
            if spent and s.get('player')==owner:
                if op=='assert_zone' and s.get('zone')=='hand' and not untapper_spent:s['value']+=1
                if op=='assert_zone' and s.get('zone')=='graveyard' and untapper_spent:s['value']+=1
                if op=='assert_mana_total' and untapper_spent:s['value']-=1
            if s.get('ref')=='source' and s.get('property') in ('power','toughness') and op in ('assert_card','require_card'):
                if counter['kind']=='minus1':s['value']-=count
                if counter['kind']=='plus1':s['value']+=count
            if spent and untapper_spent and s.get('ref')=='source' and s.get('property')=='tapped':s['value']=0
            transformed.append(s)
            if op=='activate' and s.get('ref')=='source' and s.get('value')==1:
                count+=counter['delta']
                transformed.append(step('assert_counter',ref='source',counter_kind=counter['kind'],value=count))
            if op=='destroy' and s.get('ref')=='source' and not p.get('indestructible'):alive=False
            if op in ('resolve','restore') and alive:transformed.append(step('assert_counter',ref='source',counter_kind=counter['kind'],value=count))
        if mode=='spent_counter' and not p['cost']['tap']:transformed.append(step('require_can_activate',ref='source',value=0))
        if tapped:transformed.extend([step('require_card',ref='counter_tapper',property='in_play',value=1),step('require_card',ref='counter_tapper',property='tapped',value=1)])
        if spent:transformed.append(step('assert_zone_multiplicity',player=owner,zone='graveyard',index=registry['Twiddle']['csvid'],value=1))
        case['steps']=prefix+transformed
        result.append(case)
    return result
