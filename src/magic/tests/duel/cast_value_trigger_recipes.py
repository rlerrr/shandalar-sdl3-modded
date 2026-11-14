"""Oracle-derived spell-cast predicates and value receipts through real casting."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

FAMILY = 'cast_value_trigger'
DATA = Path(__file__).parent / 'data/cast_value_trigger_wave8_profiles.json'
PIN_FIELDS = ('oracle_id', 'name', 'oracle_text', 'mana_cost', 'type_line', 'power',
              'toughness', 'keywords', 'colors', 'color_identity', 'cmc', 'layout', 'loyalty')
COLOR_CODES = {'black': 'B', 'blue': 'U', 'green': 'G', 'red': 'R', 'white': 'W'}
WORDS = {'a': 1, 'one': 1, 'two': 2, 'three': 3, 'seven': 7}
WITNESS_PAIRS = {
    'any': ('Sol Ring', 'Grizzly Bears'),
    'creature': ('Grizzly Bears', 'Ornithopter'),
    'artifact': ('Sol Ring', 'Ornithopter'),
    'enchantment': ('Seal of Cleansing', 'Seal of Fire'),
    'noncreature': ('Sol Ring', 'Seal of Cleansing'),
    'instant_or_sorcery': ('Giant Growth', 'Divination'),
    'instant': ('Giant Growth', 'Brute Force'),
    'heroic': ('Giant Growth', 'Brute Force'),
    'black': ('Scathe Zombies', 'Drudge Skeletons'),
    'blue': ('Coral Merfolk', 'Wind Drake'),
    'green': ('Grizzly Bears', 'Elvish Warrior'),
    'red': ('Goblin Piker', 'Seal of Fire'),
    'white': ('Silvercoat Lion', 'Seal of Cleansing'),
    'colorless': ('Sol Ring', 'Ornithopter'),
    'nonblack': ('Sol Ring', 'Grizzly Bears'),
    'multicolored': ('Watchwolf', 'Goblin Legionnaire'),
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def pin(record):
    return {key: record[key] for key in PIN_FIELDS if key in record}


def exact(a, b):
    if type(a) is not type(b): return False
    if isinstance(a, dict): return a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
    if isinstance(a, list): return len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
    return a == b


def classify(record):
    """Derive behavior from wording; profiles pin identity rather than define rules."""
    damage = classify_damage(record)
    if damage is not None: return damage
    extended = classify_predicate(record)
    if extended is not None: return extended
    from recipes import cost_from_oracle
    from creature_recipes import oracle_keywords
    if (not record or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Token' in record.get('type_line', '')):
        raise ValueError('normal standalone cast-trigger permanent required')
    types = set(record['type_line'].partition(' — ')[0].split())
    if not types & {'Creature', 'Artifact', 'Enchantment'}:
        raise ValueError('cast watcher must be a permanent')
    clauses = record.get('oracle_text', '').splitlines()
    triggers = [line for line in clauses if line.startswith(('Whenever ', 'Heroic — Whenever '))]
    if len(triggers) != 1:
        raise ValueError('one ordinary spell-cast trigger required')
    heroic = triggers[0].startswith('Heroic — ')
    if heroic:
        match = re.fullmatch(r'Heroic — Whenever you cast a spell that targets this creature, (.+)', triggers[0])
        if not match or 'Heroic' not in record.get('keywords', []):
            raise ValueError('complete ordinary Heroic wording and keyword required')
        scope='own'; predicate='heroic'; text=match[1]
    else:
        match = re.fullmatch(r'Whenever (you cast|a player casts|an opponent casts) (a spell|an? ([a-z ]+) spell), (.+)', triggers[0])
        if not match:
            raise ValueError('unreviewed cast-history, targeting, or trigger wording')
        scope = {'you cast': 'own', 'a player casts': 'any', 'an opponent casts': 'opponent'}[match[1]]
        predicate = 'any' if match[2] == 'a spell' else match[3].replace(' ', '_')
        text = match[4]
    if predicate not in WITNESS_PAIRS:
        raise ValueError('unreviewed spell predicate: ' + predicate)
    paid = re.fullmatch(r'you may pay ((?:\{[0-9WUBRG]+\})+)\. If you do, (.+)',text)
    trigger_cost = paid[1] if paid else None
    if paid: text = paid[2]
    optional = False
    effect = recipient = None
    amount = None
    m = re.fullmatch(r'you (may )?(gain|lose) ([1-9]) life\.', text)
    if m:
        optional = bool(m[1]); effect = m[2]; amount = int(m[3]); recipient = 'source'
    m = re.fullmatch(r'(you may )?draw (a|two|three|seven) cards?\.', text)
    if m:
        optional = bool(m[1]); effect = 'draw'; amount = WORDS[m[2]]; recipient = 'source'
    m = re.fullmatch(r'that player (mills|draws) (a|one|two|three|seven) cards?\.', text)
    if m:
        effect = 'mill' if m[1] == 'mills' else 'draw'; amount = WORDS[m[2]]; recipient = 'caster'
    m = re.fullmatch(r'that player loses ([1-9]) life\.', text)
    if m:
        effect = 'lose'; amount = int(m[1]); recipient = 'caster'
    m = re.fullmatch(r'this creature gets ([+-][1-4])/([+-][0-4]) until end of turn\.',text)
    if m:
        effect='pump'; amount=[int(m[1]),int(m[2])]; recipient='source'
    m = re.fullmatch(r'put (a|one|two|three) \+1/\+1 counters? on (?:this creature|it)\.',text)
    if m:
        effect='counters'; amount=WORDS[m[1]]; recipient='source'
    if effect is None:
        raise ValueError('unreviewed cast-trigger value effect')
    remainder = '\n'.join(line for line in clauses if line != triggers[0])
    operative_keywords = {'Mill'} if effect == 'mill' else set()
    if heroic: operative_keywords.add('Heroic')
    if effect in ('pump','counters') and 'Creature' not in types:
        raise ValueError('self stat/counter trigger requires a creature source')
    if paid and (effect not in ('gain','draw') or recipient!='source' or optional):
        raise ValueError('reviewed paid draw/life component required')
    if paid: optional=True
    if 'Creature' in types:
        keywords = oracle_keywords(dict(record, oracle_text=remainder, keywords=[k for k in record.get('keywords', []) if k not in operative_keywords]))
        power, toughness = int(record['power']), int(record['toughness'])
    else:
        if remainder or set(record.get('keywords', []))-operative_keywords:
            raise ValueError('additional permanent effects need a separate component model')
        keywords = []; power = toughness = None
    mana = [0] * 6 if record['mana_cost'] == '{0}' else cost_from_oracle(record)
    result = dict(scope=scope, predicate=predicate, effect=effect, recipient=recipient,
                amount=amount, optional=optional, mana=mana, keywords=keywords,
                power=power, toughness=toughness, legendary='Legendary' in types)
    if paid:
        result.update(mechanism='paid_cast',trigger_cost=trigger_cost,trigger_mana=cost_from_oracle(dict(mana_cost=trigger_cost)))
    elif heroic: result['mechanism']='heroic'
    elif effect in ('pump','counters'): result['mechanism']='cast_stats'
    return result


def classify_damage(record):
    if not record:return None
    lines=record.get('oracle_text','').splitlines();triggers=[s for s in lines if s.startswith('Whenever ')]
    if len(triggers)!=1 or 'damage' not in triggers[0]:return None
    m=re.fullmatch(r'Whenever (you cast|a player casts|an opponent casts) (.+?), (.+)',triggers[0])
    if not m:return None
    scope={'you cast':'own','a player casts':'any','an opponent casts':'opponent'}[m[1]];phrase=m[2];text=m[3];ordinal=0
    if phrase=='your second spell each turn' and scope=='own':expr={'op':'any'};ordinal=2
    elif phrase=='a spell':expr={'op':'any'}
    elif phrase=='a creature spell':expr={'op':'type','value':'Creature'}
    elif phrase=='an instant or sorcery spell':expr={'op':'type_any','values':['Instant','Sorcery']}
    elif phrase=='a white or blue instant or sorcery spell':expr={'op':'all','terms':[{'op':'colors_any','values':['W','U']},{'op':'type_any','values':['Instant','Sorcery']}]}
    elif phrase=='a multicolored spell':expr={'op':'multicolored'}
    elif phrase=='an Elf spell':expr={'op':'subtype_any','values':['Elf']}
    else:
        mv=re.fullmatch(r'a spell with mana value ([1-9]) or less',phrase)
        if not mv:return None
        expr={'op':'mv_max','value':int(mv[1])}
    types=set(record.get('type_line','').partition(' — ')[0].split())
    if record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or 'Token' in types or not types&{'Creature','Artifact','Enchantment'}:
        raise ValueError('normal standalone cast damage permanent required')
    optional=False;basis='fixed';amount=None;target_filter=None;recipient=None
    if text.startswith('you may have '):optional=True;text=text[len('you may have '):];verb='deal'
    else:verb='deals'
    subjects=['this creature','this enchantment',record['name'],record['name'].split()[0]]
    subject=next((s for s in sorted(subjects,key=len,reverse=True) if text.startswith(s+' '+verb+' ')),None)
    if subject is None:raise ValueError('cast damage must come from the named source')
    reward=text[len(subject+' '+verb+' '):]
    fixed=re.fullmatch(r'([1-6]) damage to (that player|each opponent|any target|target creature an opponent controls|target creature with flying)\.',reward)
    if fixed:amount=int(fixed[1]);destination=fixed[2]
    elif reward=="damage equal to that spell's mana value to any target.":amount=1;basis='mana_value';destination='any target'
    else:raise ValueError('unreviewed cast damage reward')
    recipient={'that player':'caster','each opponent':'opponents','any target':'target',
               'target creature an opponent controls':'target','target creature with flying':'target'}[destination]
    target_filter={'any target':'any','target creature an opponent controls':'opponent_creature','target creature with flying':'flying_creature'}.get(destination)
    if optional and target_filter!='flying_creature':raise ValueError('unreviewed optional cast damage presentation')
    remainder='\n'.join(s for s in lines if s!=triggers[0]);creature='Creature' in types
    from creature_recipes import oracle_keywords
    from recipes import cost_from_oracle
    if creature:keywords=oracle_keywords(dict(record,oracle_text=remainder));power=int(record['power']);toughness=int(record['toughness'])
    else:
        if remainder or record.get('keywords'):raise ValueError('additional damage source clauses require separate calibration')
        keywords=[];power=toughness=None
    return dict(scope=scope,predicate='damage_expression',effect='damage',recipient=recipient,amount=amount,optional=optional,
                mana=cost_from_oracle(record),keywords=keywords,power=power,toughness=toughness,legendary='Legendary' in types,
                mechanism='cast_damage',predicate_expr=expr,reward_basis=basis,ordinal=ordinal,target_filter=target_filter)


def damage_matches(record,expr):
    op=expr['op']
    if op=='mv_max':return record['cmc']<=expr['value']
    if op=='type_any':return bool(set(expr['values'])&set(record['type_line'].partition(' — ')[0].split()))
    if op=='colors_any':return bool(set(expr['values'])&set(record['colors']))
    if op=='multicolored':return len(record['colors'])>=2
    if op=='all':return all(damage_matches(record,e) for e in expr['terms'])
    return predicate_matches(record,expr)


def damage_variants(p):
    modes=['effect','alternate_spell','opponent_cast','repeat_same_turn','trigger_payment','trigger_insufficient_mana',
           'source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_removed_before',
           'source_arrives_late','source_removed_after_cast','countered_trigger','cleanup','bare_zones',
           'ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['ordinal']:modes+=['first_only','third_spell','countered_precursor','response_is_second','prior_turn_reset','opponent_history']
    elif p['predicate_expr']['op']!='any':modes.append('nonmatching_spell')
    if p['predicate_expr']['op']=='mv_max':modes+=['zero_value','above_threshold']
    if p['predicate_expr']['op']=='all':modes+=['sorcery_spell','nonmatching_color','overlap_once']
    if p['predicate_expr']['op']=='multicolored':modes.append('colorless_control')
    if p['reward_basis']=='mana_value':modes+=['zero_value','high_value']
    if p['target_filter']=='any':modes+=['own_player','own_creature','enemy_creature','lethal_creature','planeswalker','own_planeswalker']
    elif p['target_filter']:modes+=['lethal_creature','no_legal_target']
    if p['target_filter']=='flying_creature':modes.append('own_creature')
    if not p['legendary']:modes.append('two_sources')
    return [f'{m}_{o}' for o in (0,1) for m in modes]+(['decline_0'] if p['optional'] else [])


def damage_witnesses(p):
    op=p['predicate_expr']['op']
    if p['ordinal']:return ['Sol Ring','Grizzly Bears']
    if op=='mv_max':return ['Scathe Zombies','Sol Ring']
    if op=='type_any':return ['Giant Growth','Divination']
    if op=='type':return ['Grizzly Bears','Ornithopter']
    if op=='all':return ['Reach Through Mists','Holy Day']
    if op=='multicolored':return ['Watchwolf','Goblin Legionnaire']
    if op=='subtype_any':return ['Elvish Mystic','Elvish Warrior']
    return ['Grizzly Bears','Sol Ring']


def expand_damage(name,csvid,d,p,selected,rows,oracle):
    from recipes import card,step,observe,cost_from_oracle
    if len(set(selected))!=len(selected) or set(selected)-set(damage_variants(p)):raise ValueError('unsupported damage variants')
    results=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);actor=1-owner if p['scope']=='opponent' else owner
        if mode=='opponent_cast':actor=1-actor
        pair=damage_witnesses(p);witness=pair[mode=='alternate_spell']
        if mode in ('zero_value',):witness='Ornithopter'
        if mode=='high_value':witness='Scaled Wurm'
        if mode in ('above_threshold',):witness='Hill Giant'
        if mode=='sorcery_spell':witness='Divination'
        if mode=='overlap_once':witness='Azorius Charm'
        if mode=='colorless_control':witness='Sol Ring'
        if mode=='nonmatching_color':witness='Giant Growth'
        if mode=='nonmatching_spell':witness={'mv_max':'Hill Giant','type_any':'Sol Ring','type':'Sol Ring','all':'Coral Merfolk','multicolored':'Grizzly Bears','subtype_any':'Grizzly Bears'}[p['predicate_expr']['op']]
        source_hand=mode in ('source_cast','source_payment','source_insufficient_mana','source_wrong_color','source_arrives_late')
        denied_source=mode in ('source_insufficient_mana','source_wrong_color');denied=mode=='trigger_insufficient_mana'
        schedule=[(witness,actor,denied,mode=='countered_trigger')]
        if p['ordinal']:
            schedule=[(pair[0],actor,denied,False),(pair[1],actor,False,mode=='countered_trigger')]
            if denied or mode=='first_only':schedule=schedule[:1]
            if mode in ('repeat_same_turn','third_spell'):schedule.append(('Ornithopter',actor,False,False))
            if mode=='alternate_spell':schedule=[(pair[1],actor,False,False),(pair[0],actor,False,False)]
            if mode=='countered_precursor':schedule[0]=(*schedule[0][:3],True)
            if mode=='response_is_second':schedule=[('Sol Ring',owner,False,False),('Reach Through Mists',1-owner,False,True)]
            if mode=='opponent_history':schedule.insert(0,('Reach Through Mists',1-owner,False,False))
            if mode=='prior_turn_reset':schedule+=schedule
        elif mode=='repeat_same_turn':schedule=[(witness,actor,False,False)]*3
        setup=[];entities={};zones={z:[[],[]] for z in ('hand','graveyard','exile','library')}
        mana=[[30]*6,[30]*6];life=[20,20];history=[0,0];actions=[];pending_cut=None
        for who in (0,1):
            for z,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                zones[z][who]=list(base)*2 if z=='library' else [] if mode=='bare_zones' else list(base)
                setup += [card(n,f'zone_{who}_{z}_{i}',who,z) for i,n in enumerate(zones[z][who])]
        def add(nm,ref,who,zone='battlefield',tapped=False):
            r=oracle[nm];types=set(r['type_line'].partition(' — ')[0].split());creature='Creature' in types;obj=card(nm,ref,who,zone)
            if tapped:obj['state']=['tapped']
            setup.append(obj)
            if zone=='hand':zones['hand'][who].append(nm)
            entities[ref]=dict(name=nm,player=who,alive=zone=='battlefield',hand=zone=='hand',creature=creature,tapped=tapped,
                               power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,
                               damage=0,loyalty=int(r['loyalty']) if 'Planeswalker' in types else None,flying='Flying' in r['keywords'])
        add(name,'source',owner,'hand' if source_hand else 'battlefield');add('Hill Giant','idle',owner)
        add('Grizzly Bears','enemy',1-owner);add('Sol Ring','mana_control',owner,tapped=True)
        if mode=='two_sources':add(name,'copy',owner)
        if p['target_filter']=='flying_creature':
            add('Air Elemental','flyer_enemy',1-owner);add('Air Elemental','flyer_own',owner)
        if mode=='lethal_creature':add('Suntail Hawk' if p['target_filter']=='flying_creature' else 'Eager Cadet','lethal',1-owner)
        if mode in ('planeswalker','own_planeswalker'):add('Chandra, Pyrogenius','walker',owner if mode=='own_planeswalker' else 1-owner)
        for i,(nm,who,no,countered) in enumerate(schedule):
            add(nm,f'spell_{i}',who,'hand')
            if countered:add('Counterspell',f'response_{i}',owner if mode=='response_is_second' else 1-who,'hand')
        def cost(nm):
            if oracle[nm]['layout']!='normal' or 'Token' in oracle[nm]['type_line']:raise ValueError('normal printed damage cast witness required')
            return [0]*6 if oracle[nm]['mana_cost']=='{0}' else cost_from_oracle(oracle[nm])
        if mode=='trigger_payment':mana[actor]=[sum(cost(nm)[c] for nm,who,no,ctr in schedule if who==actor) for c in range(6)]
        if denied:mana[actor]=[0]*6
        if mode=='source_payment':mana[owner]=list(p['mana'])
        if denied_source:mana[owner]=[sum(p['mana'])]+[0]*5 if mode=='source_wrong_color' else [0]*6
        pre=[step('turn',player=actor,value=0)]+[step('set_mana',player=who,color=c,value=v) for who in (0,1) for c,v in enumerate(mana[who])]
        if any(nm=='Azorius Charm' and not no for nm,who,no,countered in schedule):pre.append(step('present_ai_dialogs',value=1))
        if 'walker' in entities:pre.append(step('assert_counter',ref='walker',counter_kind='loyalty',value=int(oracle['Chandra, Pyrogenius']['loyalty'])))
        universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
        def emit(*seq):actions.extend(seq)
        def receipts():
            seq=[step('assert_stack',value=0),step('assert_choices_consumed')]
            for ref,e in entities.items():
                if e['alive']:
                    seq += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                    if e['creature']:seq += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'damage',e['damage']),observe(ref,'plus1_counters',0)]
                    if e['loyalty'] is not None:seq.append(step('assert_counter',ref=ref,counter_kind='loyalty',value=e['loyalty']))
                elif e['hand']:seq.append(observe(ref,'in_hand',1))
            for who in (0,1):
                seq += [step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=sum(mana[who])),step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                for z in zones:
                    ns=zones[z][who];seq.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                    seq += [step('assert_zone_multiplicity',player=who,zone=z,index=rows[n]['csvid'],value=ns.count(n)) for n in sorted(universe)]
                    if z=='library':seq += [step('assert_zone_card',player=who,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)]
                if mode in ('source_payment','trigger_payment','source_insufficient_mana','source_wrong_color','trigger_insufficient_mana'):
                    seq += [step('assert_mana',player=who,color=c,value=v) for c,v in enumerate(mana[who])]
            if p['target_filter']:seq.append(step('assert_forced_target_choices',value=0))
            return seq
        def die(ref):
            e=entities[ref];e['alive']=False;zones['graveyard'][e['player']].append(e['name'])
        def legal(ref):
            e=entities[ref]
            if not e['alive']:return False
            if p['target_filter']=='any':return e['creature'] or e['loyalty'] is not None
            if p['target_filter']=='opponent_creature':return e['creature'] and e['player']!=owner
            return e['creature'] and e['flying']
        def target():
            if p['target_filter']=='any' and mode not in ('own_creature','enemy_creature','lethal_creature','planeswalker','own_planeswalker'):
                return (owner if mode=='own_player' else 1-owner),None
            preferred='walker' if mode in ('planeswalker','own_planeswalker') else 'lethal' if mode=='lethal_creature' else ('flyer_own' if p['target_filter']=='flying_creature' else 'idle') if mode=='own_creature' else 'flyer_enemy' if p['target_filter']=='flying_creature' else 'enemy'
            refs=[preferred]+[r for r in entities if r!=preferred]
            ref=next((r for r in refs if r in entities and legal(r)),None)
            return (entities[ref]['player'],ref) if ref else (None,None)
        def damage(who,ref,amount):
            if ref is None:life[who]-=amount;return
            e=entities[ref]
            if e['loyalty'] is not None:
                e['loyalty']-=min(e['loyalty'],amount)
                if not e['loyalty']:die(ref)
            else:
                e['damage']+=amount
                if e['damage']>=e['toughness']:die(ref)
        def notify(nm,who):
            history[who]+=1
            if p['scope']=='own' and who!=owner or p['scope']=='opponent' and who==owner:return False
            if p['ordinal'] and history[who]!=p['ordinal'] or not damage_matches(oracle[nm],p['predicate_expr']):return False
            planned=False
            for source in ('source','copy'):
                if source not in entities or not entities[source]['alive']:continue
                amount=p['amount']*(int(oracle[nm]['cmc']) if p['reward_basis']=='mana_value' else 1)
                if p['recipient']=='target':
                    recipient,ref=target()
                    if recipient is None:continue
                    if p['optional'] and owner==0:
                        if p.get('damage_target_presentation'):
                            if mode=='decline':
                                emit(step('choose_optional_target',player=owner,ref=ref));planned=True;continue
                        else:
                            if mode=='decline':continue
                            emit(step('choose_trigger_source',player=owner,ref=source))
                    emit(step('choose_target',player=owner,ref=ref) if ref else step('choose_player',player=owner,value=recipient));planned=True
                    damage(recipient,ref,amount)
                elif p['recipient']=='caster':damage(who,None,amount)
                else:damage(1-owner,None,amount)
            return planned
        def draw(who,n):
            if len(zones['library'][who])<n:raise ValueError('insufficient damage witness library')
            zones['hand'][who]+=zones['library'][who][:n];del zones['library'][who][:n]
        def start(ref,no=False,response=False):
            e=entities[ref];nm=e['name'];who=e['player']
            if nm=='Giant Growth' and not no:emit(step('choose_target',player=who,ref='idle'))
            if nm=='Azorius Charm' and not no:emit(step('choose_dialog',player=who,value=1))
            emit(step('cast_response' if response else 'cast',ref=ref,value=int(not no)))
            if no:emit(step('assert_stack',value=0),step('assert_choices_consumed'));return None
            mana[who]=[a-b for a,b in zip(mana[who],cost(nm))]
            if min(mana[who])<0:raise ValueError('damage cast underfunded: '+nm)
            e['hand']=False;zones['hand'][who].remove(nm)
            emit(step('assert_stack',value=2 if response else 1),observe(ref,'in_hand',0));planned=notify(nm,who)
            probe=0 if planned else None
            if not planned and p['target_filter']:
                live=[s for s in ('source','copy') if s in entities and entities[s]['alive']]
                fallback=next((r for r in entities if legal(r)),None)
                if live and fallback:
                    emit(*(step('choose_optional_target',player=owner,ref=fallback) for s in live));probe=len(live)
            if not response:
                emit(step('announce_spell',ref=ref))
                if probe is not None:emit(step('assert_unused_choices',value=probe))
            return probe
        def finish(ref,countered=False):
            e=entities[ref];nm=e['name'];who=e['player'];types=set(oracle[nm]['type_line'].partition(' — ')[0].split())
            if countered or not types&{'Creature','Artifact','Enchantment','Planeswalker'}:
                zones['graveyard'][who].append(nm)
                if not countered:
                    if nm=='Divination':draw(who,2)
                    if nm=='Reach Through Mists':draw(who,1)
                    if nm=='Azorius Charm':draw(who,1)
                    if nm=='Giant Growth':entities['idle']['power']+=3;entities['idle']['toughness']+=3
            else:e['alive']=True
        def cast_source():
            start('source',denied_source)
            if not denied_source:
                emit(step('resolve'),step('assert_choices_consumed'));finish('source')
                if mode=='source_payment':
                    emit(*receipts());mana[owner]=[30]*6;emit(*(step('set_mana',player=owner,color=c,value=v) for c,v in enumerate(mana[owner])))
        if source_hand and mode!='source_arrives_late':cast_source()
        if mode=='source_removed_before':emit(step('destroy',ref='source'));die('source')
        if mode=='no_legal_target':
            for ref,e in entities.items():
                if legal(ref):emit(step('destroy',ref=ref));die(ref)
        if not denied_source:
            for i,(nm,who,no,countered) in enumerate(schedule):
                if mode=='prior_turn_reset' and i==2:
                    emit(*receipts());emit(*(step('set_mana',player=pl,color=c,value=0) for pl in (0,1) for c in range(6)))
                    mana=[[0]*6,[0]*6];emit(step('advance_turn'),step('advance_turn'));history=[0,0]
                    for e in entities.values():
                        if e['creature'] and e['alive']:e['power']=int(oracle[e['name']]['power']);e['toughness']=int(oracle[e['name']]['toughness']);e['damage']=0
                    mana=[[30]*6,[30]*6];emit(*(step('set_mana',player=pl,color=c,value=v) for pl in (0,1) for c,v in enumerate(mana[pl])))
                ref=f'spell_{i}';start(ref,no)
                if no:emit(*receipts());continue
                if pending_cut is None:pending_cut=len(actions)
                if mode=='source_removed_after_cast' and entities['source']['alive'] and (not p['ordinal'] or history[who]==p['ordinal']):emit(step('destroy',ref='source'));die('source')
                if countered:
                    response=f'response_{i}';probe=start(response,response=True)
                    emit(step('resolve_response'))
                    if probe is not None:emit(step('assert_unused_choices',value=probe))
                    emit(step('assert_choices_consumed'));finish(response);finish(ref,True)
                else:emit(step('resolve'),step('assert_choices_consumed'));finish(ref)
                emit(*receipts())
            if mode=='source_arrives_late':cast_source()
        if mode=='cleanup':
            emit(*(step('assert_mana_total',player=who,value=sum(mana[who])) for who in (0,1)))
            emit(*(step('set_mana',player=who,color=c,value=0) for who in (0,1) for c in range(6)));mana=[[0]*6,[0]*6];emit(step('cleanup'))
            for e in entities.values():
                if e['creature'] and e['alive']:e['damage']=0;e['power']=int(oracle[e['name']]['power']);e['toughness']=int(oracle[e['name']]['toughness'])
        checks=receipts();seq=pre+actions+checks
        if mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if mode=='ready_snapshot' else pending_cut
            seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        gaps=['Card-specific forbidden-trigger-target queries and responses after a separately pending target-committed trigger are not exposed by this fixture; no synthetic windows or illegal target injections.',
              'Damage prevention/redirection/replacements, multiplayer, dynamic X, spell copies and keyword combat.',
              'Kindred Elf noncreature witnesses require their independent token ledger and are not claimed here.',
              'Player, creature and planeswalker are the target categories represented in this pinned Oracle database; it contains no Battle identities.',
              'Actual paid mana is checked before explicit pool emptying for cleanup; phase-boundary mana expiry is outside the helper.']
        results.append(dict(name=f'CastValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,
                            _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
    return results


def classify_predicate(record):
    """Complete new wording only; the established grammar is left untouched."""
    if not record: return None
    clauses=record.get('oracle_text','').splitlines()
    triggers=[s for s in clauses if s.startswith('Whenever ')]
    if len(triggers)!=1: return None
    text=triggers[0];ordinal=0;expr=None
    m=re.fullmatch(r'Whenever you cast your second spell each turn, (.+)',text)
    if m:expr={'op':'any'};ordinal=2;effect_text=m[1]
    if expr is None:
        m=re.fullmatch(r'Whenever you cast (a Spirit or Arcane spell|an Aura, Equipment, or Vehicle spell|a historic spell|a legendary spell|an Eldrazi creature spell with mana value ([1-9]) or greater|a spell), (.+)',text)
        if not m:return None
        phrase=m[1];effect_text=m[3]
        if phrase=='a Spirit or Arcane spell':expr={'op':'subtype_any','values':['Spirit','Arcane']}
        elif phrase=='an Aura, Equipment, or Vehicle spell':expr={'op':'subtype_any','values':['Aura','Equipment','Vehicle']}
        elif phrase=='a historic spell':expr={'op':'historic'}
        elif phrase=='a legendary spell':expr={'op':'supertype','value':'Legendary'}
        elif m[2]:expr={'op':'all','terms':[{'op':'type','value':'Creature'},{'op':'subtype_any','values':['Eldrazi']},{'op':'mv_min','value':int(m[2])}]}
        elif re.fullmatch(r"this creature gets \+X/\+X until end of turn, where X is that spell's mana value\.",effect_text):expr={'op':'any'}
        else:return None
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Token' in record.get('type_line','') or not set(record['type_line'].partition(' — ')[0].split())&{'Creature','Artifact','Enchantment'}):
        raise ValueError('normal standalone predicate watcher required')
    optional=False;basis='fixed';amount=None;effect=None
    m=re.fullmatch(r'draw (a|two) cards?\.(?: \(Artifacts, legendaries, and Sagas are historic\.\))?',effect_text)
    if m:effect='draw';amount=WORDS[m[1]]
    m=re.fullmatch(r'this creature gets (\+[1-4])/(\+[1-4]) until end of turn\.',effect_text)
    if m:effect='pump';amount=[int(m[1]),int(m[2])]
    if effect_text=="you may gain life equal to that spell's mana value.":effect='gain';amount=1;basis='mana_value';optional=True
    if effect_text=="this creature gets +X/+X until end of turn, where X is that spell's mana value.":effect='pump';amount=[1,1];basis='mana_value'
    if effect is None:raise ValueError('unreviewed predicate value reward')
    if expr['op']=='historic' and not effect_text.endswith('(Artifacts, legendaries, and Sagas are historic.)'):
        raise ValueError('complete historic reminder required')
    from creature_recipes import oracle_keywords
    from recipes import cost_from_oracle
    creature='Creature' in record['type_line'].partition(' — ')[0].split()
    remainder='\n'.join(s for s in clauses if s!=text)
    if creature:
        keywords=oracle_keywords(dict(record,oracle_text=remainder));power=int(record['power']);toughness=int(record['toughness'])
    else:
        if remainder or record.get('keywords'):raise ValueError('extra predicate source component')
        keywords=[];power=toughness=None
    if effect=='pump' and not creature:raise ValueError('pump requires creature source')
    return dict(scope='own',predicate='expression',effect=effect,recipient='source',amount=amount,optional=optional,
                mana=[0]*6 if record['mana_cost']=='{0}' else cost_from_oracle(record),keywords=keywords,power=power,toughness=toughness,
                legendary='Legendary' in record['type_line'].partition(' — ')[0].split(),mechanism='predicate_value',
                predicate_expr=expr,reward_basis=basis,ordinal=ordinal)


def predicate_matches(record,expr):
    head,_,tail=record['type_line'].partition(' — ')
    op=expr['op']
    if op=='any':return True
    if op=='type' or op=='supertype':return expr['value'] in head.split()
    if op=='subtype_any':return bool(set(expr['values'])&set(tail.split()))
    if op=='mv_min':return record['cmc']>=expr['value']
    if op=='historic':return 'Artifact' in head.split() or 'Legendary' in head.split() or 'Saga' in tail.split()
    if op=='all':return all(predicate_matches(record,e) for e in expr['terms'])
    raise ValueError('unreviewed predicate expression')


def predicate_variants(p):
    modes=['effect','alternate_spell','opponent_cast','repeat_same_turn','trigger_payment','trigger_insufficient_mana',
           'source_cast','source_payment','source_insufficient_mana','source_removed_before','source_arrives_late',
           'source_removed_after_cast','countered_trigger','cleanup','bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot']
    if any(p['mana'][1:]):modes.append('source_wrong_color')
    expr=p['predicate_expr'];op=expr['op']
    if p['ordinal']:modes+=['first_only','third_spell','prior_turn_reset','countered_precursor','opponent_history','response_is_second']
    elif op=='any':modes+=['zero_value','high_value','mixed_values']
    elif op=='all':modes+=['below_threshold','nonmatching_spell','noncreature_subtype']
    else:
        modes.append('nonmatching_spell')
        if expr.get('values')==['Spirit','Arcane']:modes+=['nonmatching_instant','higher_value']
        if expr.get('values')==['Aura','Equipment','Vehicle']:modes+=['vehicle_spell','nonmatching_enchantment']
        if op=='historic':modes+=['saga_spell','overlap_once','nonmatching_enchantment']
        if op=='supertype':modes+=['legendary_artifact','nonmatching_artifact']
    if not p['legendary']:modes.append('two_sources')
    return [f'{m}_{o}' for o in (0,1) for m in modes]+(['decline_0'] if p['optional'] else [])


def predicate_witnesses(p):
    expr=p['predicate_expr'];op=expr['op']
    if p['ordinal']:return ['Sol Ring','Grizzly Bears']
    if op=='any':return ['Grizzly Bears','Sol Ring']
    if op=='all':return ['Bane of Bala Ged','Eldrazi Devastator']
    if expr.get('values')==['Spirit','Arcane']:return ['Wandering Ones','Reach Through Mists']
    if expr.get('values')==['Aura','Equipment','Vehicle']:return ['Bonesplitter','Pacifism']
    if op=='historic':return ['Sol Ring','Isamaru, Hound of Konda']
    if op=='supertype':return ['Isamaru, Hound of Konda','Jasmine Boreal']
    raise ValueError('unreviewed predicate witness cohort')


def expand_predicate(name,csvid,d,p,selected,rows,oracle):
    """Independent Oracle ledger for the new expressions and real cast history."""
    from recipes import card,step,observe,cost_from_oracle
    if len(set(selected))!=len(selected) or set(selected)-set(predicate_variants(p)):raise ValueError('unsupported predicate variants')
    results=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner);actor=1-owner if mode=='opponent_cast' else owner
        pair=predicate_witnesses(p);witness=pair[mode=='alternate_spell']
        overrides={'zero_value':'Ornithopter','high_value':'Scaled Wurm','below_threshold':'Deathless Behemoth',
                   'noncreature_subtype':'Eldrazi Conscription','vehicle_spell':"Smuggler's Copter",'saga_spell':'Song of Freyalise',
                   'overlap_once':'Hope of Ghirapur','nonmatching_enchantment':'Seal of Cleansing'}
        overrides.update(nonmatching_instant='Dark Ritual',higher_value='Kami of Ancient Law',legendary_artifact='Mirari',nonmatching_artifact='Sol Ring')
        if mode in overrides:witness=overrides[mode]
        if mode=='nonmatching_spell':witness='Scaled Wurm' if p['predicate_expr']['op']=='all' else 'Grizzly Bears'
        if mode=='nonmatching_spell' and p['predicate_expr'].get('values')==['Aura','Equipment','Vehicle']:witness='Sol Ring'
        if mode=='nonmatching_spell' and p['predicate_expr'].get('values')==['Spirit','Arcane']:witness='Grizzly Bears'
        source_hand=mode.startswith('source_') and mode not in ('source_removed_before','source_removed_after_cast')
        denied_source=mode in ('source_insufficient_mana','source_wrong_color')
        denied_witness=mode=='trigger_insufficient_mana'
        schedule=[(witness,actor,denied_witness,False)]
        if p['ordinal']:
            schedule=[(pair[0],actor,denied_witness,False),(pair[1],actor,False,False)]
            if mode=='alternate_spell':schedule=[(pair[1],actor,False,False),(pair[0],actor,False,False)]
            if denied_witness:schedule=schedule[:1]
            if mode=='first_only':schedule=schedule[:1]
            if mode in ('repeat_same_turn','third_spell'):schedule.append(('Ornithopter',actor,False,False))
            if mode=='opponent_history':schedule.insert(0,('Reach Through Mists',1-owner,False,False))
            if mode=='countered_precursor':schedule[0]=(*schedule[0][:3],True)
            if mode=='countered_trigger':schedule[1]=(*schedule[1][:3],True)
            if mode=='response_is_second':schedule=[('Sol Ring',owner,False,False),('Reach Through Mists',1-owner,False,True)]
        elif mode=='repeat_same_turn':
            names=[witness]*3 if p['predicate_expr']['op']!='supertype' else ['Isamaru, Hound of Konda','Jasmine Boreal','Tobias Andrion']
            schedule=[(nm,actor,False,False) for nm in names]
        elif mode=='mixed_values':schedule=[('Ornithopter',actor,False,False),('Sol Ring',actor,False,False),('Grizzly Bears',actor,False,False)]
        if mode=='countered_trigger' and not p['ordinal']:schedule[0]=(*schedule[0][:3],True)
        if mode=='prior_turn_reset':schedule=schedule+schedule
        setup=[];entities={};mana=[[30]*6 for _ in (0,1)];life=[20,20];history=[0,0]
        zones={z:[[],[]] for z in ('hand','graveyard','exile','library')}
        for who in (0,1):
            for z,base in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                zones[z][who]=list(base)*4 if z=='library' else [] if mode=='bare_zones' else list(base)
                setup += [card(n,f'zone_{who}_{z}_{i}',who,z) for i,n in enumerate(zones[z][who])]
        def add(nm,ref,who,zone='battlefield',tapped=False):
            r=oracle[nm];creature='Creature' in r['type_line'].partition(' — ')[0].split();obj=card(nm,ref,who,zone)
            if tapped:obj['state']=['tapped']
            setup.append(obj)
            if zone=='hand':zones['hand'][who].append(nm)
            entities[ref]=dict(name=nm,player=who,alive=zone=='battlefield',hand=zone=='hand',creature=creature,tapped=tapped,
                               power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
        add(name,'source',owner,'hand' if source_hand else 'battlefield')
        add('Hill Giant','idle',owner);add('Grizzly Bears','enemy',1-owner);add('Sol Ring','mana_control',owner,tapped=True)
        if mode=='two_sources':add(name,'copy',owner)
        for i,(nm,who,denied,countered) in enumerate(schedule):
            add(nm,f'spell_{i}',who,'hand')
            if countered:add('Counterspell',f'response_{i}',owner if mode=='response_is_second' else 1-who,'hand')
        def cost(nm):return [0]*6 if oracle[nm]['mana_cost']=='{0}' else cost_from_oracle(oracle[nm])
        if mode=='trigger_payment':mana[actor]=[sum(cost(nm)[c] for nm,who,denied,countered in schedule if who==actor) for c in range(6)]
        if denied_witness:mana[actor]=[0]*6
        if mode=='source_payment':mana[owner]=list(p['mana'])
        if denied_source:mana[owner]=[sum(p['mana'])]+[0]*5 if mode=='source_wrong_color' else [0]*6
        pre=[step('turn',player=actor,value=0)]+[step('set_mana',player=who,color=c,value=v) for who in (0,1) for c,v in enumerate(mana[who])]
        actions=[];pending_cut=None;universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
        def emit(*steps):actions.extend(steps)
        def receipts():
            seq=[step('assert_stack',value=0),step('assert_choices_consumed')]
            for ref,e in entities.items():
                if e['alive']:
                    seq += [observe(ref,'in_play',1),observe(ref,'in_hand',0),observe(ref,'tapped',int(e['tapped']))]
                    if e['creature']:seq += [observe(ref,'power',e['power']),observe(ref,'toughness',e['toughness']),observe(ref,'plus1_counters',0),observe(ref,'damage',0)]
                elif e['hand']:seq.append(observe(ref,'in_hand',1))
            for who in (0,1):
                seq += [step('assert_life',player=who,value=life[who]),step('assert_mana_total',player=who,value=sum(mana[who])),
                        step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                for z in zones:
                    ns=zones[z][who];seq.append(step('assert_zone',player=who,zone=z,value=len(ns)))
                    seq += [step('assert_zone_multiplicity',player=who,zone=z,index=rows[n]['csvid'],value=ns.count(n)) for n in sorted(universe)]
                    if z=='library':seq += [step('assert_zone_card',player=who,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)]
                if mode in ('source_payment','trigger_payment','trigger_insufficient_mana','source_insufficient_mana','source_wrong_color'):
                    seq += [step('assert_mana',player=who,color=c,value=v) for c,v in enumerate(mana[who])]
            return seq
        def draw(who,n):
            if len(zones['library'][who])<n:raise ValueError('insufficient predicate draw fixture')
            zones['hand'][who]+=zones['library'][who][:n];del zones['library'][who][:n]
        def notify(nm,who):
            history[who]+=1
            if who!=owner or p['ordinal'] and history[who]!=p['ordinal'] or not predicate_matches(oracle[nm],p['predicate_expr']):return
            for ref in ('source','copy'):
                if ref not in entities or not entities[ref]['alive']:continue
                if p['optional'] and owner==0:
                    if mode=='decline':continue
                    emit(step('choose_trigger',player=owner,ref=ref))
                scale=int(oracle[nm]['cmc']) if p['reward_basis']=='mana_value' else 1
                if p['effect']=='draw':draw(owner,p['amount']*scale)
                elif p['effect']=='gain':life[owner]+=p['amount']*scale
                else:entities[ref]['power']+=p['amount'][0]*scale;entities[ref]['toughness']+=p['amount'][1]*scale
        def start_cast(ref,denied=False):
            e=entities[ref];nm=e['name'];who=e['player'];emit(step('cast',ref=ref,value=int(not denied)))
            if denied:
                emit(step('assert_stack',value=0),step('assert_choices_consumed'));return False
            mana[who]=[a-b for a,b in zip(mana[who],cost(nm))]
            if min(mana[who])<0:raise ValueError('predicate cast underfunded: '+nm)
            e['hand']=False;zones['hand'][who].remove(nm)
            emit(step('assert_stack',value=1),observe(ref,'in_hand',0));notify(nm,who)
            emit(step('announce_spell',ref=ref));return True
        def finish(ref,countered=False):
            e=entities[ref];nm=e['name'];who=e['player']
            if countered or not set(oracle[nm]['type_line'].partition(' — ')[0].split())&{'Creature','Artifact','Enchantment'}:
                zones['graveyard'][who].append(nm)
                if not countered and nm=='Reach Through Mists':draw(who,1)
                if not countered and nm=='Dark Ritual':mana[who][1]+=3
            else:
                e['alive']=True
                if nm=='Eldrazi Conscription':entities['idle']['power']+=10;entities['idle']['toughness']+=10
        def cast_source():
            if start_cast('source',denied_source):
                emit(step('resolve'),step('assert_choices_consumed'));finish('source')
                if mode=='source_payment':
                    emit(*receipts());mana[owner]=[20]*6
                    emit(*(step('set_mana',player=owner,color=c,value=v) for c,v in enumerate(mana[owner])))
        if source_hand and mode!='source_arrives_late':cast_source()
        if mode=='source_removed_before':
            emit(step('destroy',ref='source'));entities['source']['alive']=False;zones['graveyard'][owner].append(name)
        if not denied_source:
            for i,(nm,who,denied,countered) in enumerate(schedule):
                if mode=='prior_turn_reset' and i==2:
                    emit(*receipts())
                    for pl in (0,1):
                        emit(*(step('set_mana',player=pl,color=c,value=0) for c in range(6)));mana[pl]=[0]*6
                    emit(step('advance_turn'),step('advance_turn'));history=[0,0]
                    for e in entities.values():
                        if e['creature'] and e['alive']:e['power']=int(oracle[e['name']]['power']);e['toughness']=int(oracle[e['name']]['toughness'])
                    for pl in (0,1):
                        mana[pl]=[20]*6;emit(*(step('set_mana',player=pl,color=c,value=v) for c,v in enumerate(mana[pl])))
                ref=f'spell_{i}'
                if nm in ('Pacifism','Eldrazi Conscription') and not denied:emit(step('choose_target',player=who,ref='idle'))
                if not start_cast(ref,denied):
                    emit(*receipts());continue
                if pending_cut is None:pending_cut=len(actions)
                if mode=='source_removed_after_cast' and entities['source']['alive'] and (not p['ordinal'] or history[who]==p['ordinal']):
                    emit(step('destroy',ref='source'));entities['source']['alive']=False;zones['graveyard'][owner].append(name)
                if countered:
                    response=f'response_{i}';r=entities[response];rw=r['player']
                    emit(step('cast_response',ref=response,value=1),step('assert_stack',value=2))
                    mana[rw]=[a-b for a,b in zip(mana[rw],cost('Counterspell'))];r['hand']=False;zones['hand'][rw].remove('Counterspell')
                    notify('Counterspell',rw)
                    emit(step('resolve_response'),step('assert_choices_consumed'));finish(response);finish(ref,True)
                else:emit(step('resolve'),step('assert_choices_consumed'));finish(ref)
                emit(*receipts())
            if mode=='source_arrives_late':cast_source()
        if mode=='cleanup':
            emit(*(step('assert_mana_total',player=who,value=sum(mana[who])) for who in (0,1)))
            emit(*(step('set_mana',player=who,color=c,value=0) for who in (0,1) for c in range(6)));mana=[[0]*6,[0]*6]
            emit(step('cleanup'))
            for e in entities.values():
                if e['creature'] and e['alive']:e['power']=int(oracle[e['name']]['power']);e['toughness']=int(oracle[e['name']]['toughness'])
        checks=receipts();seq=pre+actions+checks
        if mode in ('ready_snapshot','pending_snapshot'):
            cut=0 if mode=='ready_snapshot' else pending_cut
            seq=pre+actions[:cut]+[step('capture')]+actions[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions[cut:]+checks+[step('assert_recorded_state')]
        if mode=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        results.append(dict(name=f'CastValueTriggerRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=seq,
                            _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                                         untested_components=['Dynamic X, spell copies, splice, chosen-subtype menus, multiplayer, replacement effects and keyword combat.',
                                                              'Saga witness exercises chapter I only; no automatic turn advancement in that branch.',
                                                              'Cleanup does not model mana expiry; actual paid balances precede explicit empty pools.'])))
    return results


def parameters(p):
    result={k:p[k] for k in ('scope','predicate','effect','recipient','amount','optional')}
    for key in ('mechanism','trigger_cost'):
        if key in p:result[key]=p[key]
    if p.get('mechanism') == 'predicate_value':
        result.update(predicate_expr=copy.deepcopy(p['predicate_expr']), reward_basis=p['reward_basis'], ordinal=p['ordinal'])
    if p.get('mechanism') == 'cast_damage':
        result.update(predicate_expr=copy.deepcopy(p['predicate_expr']), reward_basis=p['reward_basis'], ordinal=p['ordinal'], target_filter=p['target_filter'])
    return result


def spec(declaration, record):
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record.get('oracle_text')):
        raise ValueError('cast-value trigger requires exact family/version and full Oracle text')
    q = pins()['sources'].get(record.get('name'))
    if not q or not exact(pin(record), q):
        raise ValueError('cast-value trigger source identity changed')
    derived = classify(record)
    expected = parameters(derived)
    if not exact(declaration.get('params'), expected):
        raise ValueError('cast-value trigger parameters must derive exactly from Oracle grammar')
    presentation=pins().get('presentation',{}).get(record.get('name'))
    if presentation is not None:
        if (set(presentation)!={'handler','acceptance','decline','decline_witness','payment_land'}
                or derived.get('trigger_mana')!=[1,0,0,0,0,0]
                or derived['scope']!='any' or derived['predicate']!='artifact'
                or presentation.get('acceptance')!='payment'
                or presentation.get('decline')!='cancel_mana_with_land'):
            raise ValueError('reviewed optional-payment presentation required')
        derived['presentation']=copy.deepcopy(presentation)
    if 'presentation' in declaration:
        raise ValueError('unreviewed cast-trigger presentation')
    damage_presentation=pins().get('damage_presentations',{}).get(record.get('name'))
    if damage_presentation is not None:
        if (set(damage_presentation)!={'handler','acceptance','decline'}
                or derived.get('mechanism')!='cast_damage' or not derived['optional']
                or derived.get('target_filter')!='flying_creature'
                or damage_presentation['acceptance']!='target' or damage_presentation['decline']!='cancel_target'):
            raise ValueError('reviewed optional damage target presentation required')
        derived['damage_target_presentation']=copy.deepcopy(damage_presentation)
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(x, str) for x in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(derived))):
        raise ValueError('explicit distinct supported cast-value variants required')
    return dict(derived, family=FAMILY)


def matches(record, predicate):
    types = set(record['type_line'].partition(' — ')[0].split())
    if predicate == 'any': return True
    if predicate in ('creature', 'artifact', 'enchantment','instant'): return predicate.capitalize() in types
    if predicate == 'noncreature': return 'Creature' not in types
    if predicate == 'instant_or_sorcery': return bool(types & {'Instant', 'Sorcery'})
    if predicate == 'heroic':return bool(types & {'Instant','Sorcery'})
    colors = record['colors']
    if predicate in COLOR_CODES: return COLOR_CODES[predicate] in colors
    if predicate == 'colorless': return not colors
    if predicate == 'nonblack': return 'B' not in colors
    if predicate == 'multicolored': return len(colors) >= 2
    raise ValueError('unsupported cast predicate')


def variants(p):
    if p.get('mechanism') == 'cast_damage': return damage_variants(p)
    if p.get('mechanism') == 'predicate_value': return predicate_variants(p)
    common = ['effect', 'alternate_spell', 'opponent_cast', 'repeat_same_turn',
              'trigger_payment', 'trigger_insufficient_mana', 'source_cast', 'source_payment',
              'source_insufficient_mana', 'source_removed_before', 'source_arrives_late',
              'source_removed_after_cast', 'countered_trigger', 'cleanup', 'bare_zones',
              'ready_snapshot', 'pending_snapshot', 'resolved_snapshot']
    if p['predicate'] != 'any': common.append('nonmatching_spell')
    if any(p['mana'][1:]): common.append('source_wrong_color')
    if not p['optional'] and not p['legendary']: common.append('two_sources')
    if p['effect'] == 'mill': common += ['short_library', 'empty_library']
    if p.get('trigger_cost'):common.append('trigger_unaffordable')
    if p['predicate']=='heroic':common.append('other_target')
    if p['predicate']=='instant':common.append('sorcery_control')
    selected=[f'{mode}_{owner}' for owner in (0, 1) for mode in common] + (['decline_0'] if p['optional'] else [])
    if p.get('trigger_cost'):selected.append('alternate_decline_0')
    return selected


def validate_witnesses(oracle, rows):
    from creature_recipes import oracle_keywords
    data = pins()
    for name, presentation in data.get('damage_presentations',{}).items():
        if name not in data['sources'] or rows.get(name,{}).get('handler')!=presentation.get('handler'):
            raise ValueError('optional damage presentation handler changed')
    for name, mode in data.get('witness_modes', {}).items():
        if (name not in data['witnesses'] or rows.get(name,{}).get('handler')!=mode.get('handler')
                or mode.get('label') not in oracle.get(name,{}).get('oracle_text','')
                or type(mode.get('index')) is not int or mode['index']!=1):
            raise ValueError('reviewed witness cast-time mode changed')
    for name, expected in dict(data['witnesses'], **data['sources']).items():
        record = oracle.get(name, {})
        if (name not in rows or not exact(pin(record), expected) or record.get('card_faces')
                or record.get('selected_face') or rows[name]['handler'] in ('NULL', '0')
                or rows[name]['handler'] == 'card_dummy' and name in data['sources']):
            raise ValueError('cast-value trigger witness/source changed: ' + name)
        if rows[name]['handler'] == 'card_dummy':
            # Vanilla bodies and database keywords legitimately use this
            # handler; never accept a missing active witness implementation.
            oracle_keywords(record)
    for name,presentation in data.get('presentation',{}).items():
        if name not in data['sources'] or rows[name]['handler']!=presentation['handler']:
            raise ValueError('optional-payment presentation handler changed')
        witness=presentation['decline_witness'];land=presentation['payment_land']
        if (witness not in data['witnesses'] or land not in data['witnesses']
                or oracle[witness]['mana_cost']!='{0}'
                or not all(t in oracle[witness]['type_line'] for t in ('Artifact','Creature'))
                or oracle[land]['type_line']!='Basic Land — Forest'):
            raise ValueError('reviewed real optional-payment witnesses required')


def expand(name, csvid, d, p, selected, rows, oracle):
    if p.get('mechanism') == 'cast_damage': return expand_damage(name, csvid, d, p, selected, rows, oracle)
    if p.get('mechanism') == 'predicate_value': return expand_predicate(name, csvid, d, p, selected, rows, oracle)
    from recipes import card, step, observe, cost_from_oracle
    result = []
    for variant in selected:
        mode, owner_text = variant.rsplit('_', 1); owner = int(owner_text)
        bare = mode == 'bare_zones'
        actor = 1 - owner if p['scope'] == 'opponent' else owner
        if mode == 'opponent_cast': actor = 1 - actor
        payment_ui=p.get('presentation',{}).get('acceptance')=='payment'
        payment_decline=payment_ui and owner==0 and mode in ('decline','alternate_decline')
        if payment_decline and mode=='alternate_decline':actor=1-owner
        witness = WITNESS_PAIRS[p['predicate']][mode in ('alternate_spell','alternate_decline')]
        if payment_decline:witness=p['presentation']['decline_witness']
        if mode=='sorcery_control':witness='Divination'
        if mode == 'nonmatching_spell':
            witness = next(n for n in ('Grizzly Bears', 'Sol Ring', 'Scathe Zombies', 'Seal of Cleansing') if not matches(oracle[n], p['predicate']))
        count = 3 if mode == 'repeat_same_turn' else 1
        cast_source = mode in ('source_cast', 'source_payment', 'source_insufficient_mana', 'source_wrong_color', 'source_arrives_late')
        denied_source = mode in ('source_insufficient_mana', 'source_wrong_color')
        denied_witness = mode == 'trigger_insufficient_mana'
        offer_probe = (mode=='opponent_cast' and p['scope']=='any' and p['predicate']=='artifact'
                       and p['optional'] and not p.get('trigger_cost'))
        mana = [[20] * 6, [20] * 6]; life = [20, 20]
        zones = {z: [[], []] for z in ('hand', 'graveyard', 'exile', 'library')}
        for who in (0, 1):
            for z, base in (('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE), ('library', LIBRARY)):
                library_copies = max(1, (3*p['amount']+9)//7) if p['effect'] in ('draw','mill') else 1
                zones[z][who] = ([] if bare else list(base)) if z != 'library' else list(LIBRARY) * library_copies
        recipient = owner if p['recipient'] == 'source' else actor
        if mode in ('short_library', 'empty_library'):
            zones['library'][recipient] = ['Forest'] if mode == 'short_library' else []
        setup = []; entities = {}
        for who in (0, 1):
            for z in zones:
                setup += [card(n, f'zone_{who}_{z}_{i}', who, z) for i, n in enumerate(zones[z][who])]

        def add(nm, ref, who, zone='battlefield', tapped=False):
            obj = card(nm, ref, who, zone)
            if tapped: obj['state'] = ['tapped']
            setup.append(obj)
            if zone == 'hand': zones['hand'][who].append(nm)
            r = oracle[nm]; creature = 'Creature' in r['type_line'].partition(' — ')[0].split()
            entities[ref] = dict(name=nm, player=who, alive=zone == 'battlefield', hand=zone == 'hand',
                                 pending=False, creature=creature, tapped=tapped,
                                 power=int(r['power']) if creature else None,
                                 toughness=int(r['toughness']) if creature else None,counters=0)

        add(name, 'source', owner, 'hand' if cast_source else 'battlefield')
        add('Hill Giant', 'idle', owner); add('Grizzly Bears', 'enemy', 1-owner)
        add('Sol Ring', 'mana_control', owner, tapped=True)
        if payment_decline:add(p['presentation']['payment_land'],'optional_payment_land',owner)
        if mode == 'two_sources': add(name, 'copy', owner)
        for i in range(count): add(witness, f'spell_{i}', actor, 'hand')
        if mode == 'countered_trigger': add('Counterspell', 'response', 1-actor, 'hand')
        source_cost = p['mana']
        witness_cost = [0]*6 if oracle[witness]['mana_cost'] == '{0}' else cost_from_oracle(oracle[witness])
        if mode == 'source_payment': mana[owner] = list(source_cost)
        if denied_source: mana[owner] = [sum(source_cost)] + [0]*5 if mode == 'source_wrong_color' else [0]*6
        if mode == 'trigger_payment': mana[actor] = list(witness_cost)
        if mode == 'trigger_payment' and p.get('trigger_cost'):
            mana[owner]=[a+b for a,b in zip(mana[owner] if owner==actor else [0]*6,p['trigger_mana'])]
        if mode=='trigger_unaffordable':
            mana[owner]=list(witness_cost) if owner==actor else [0]*6
        if payment_decline:mana[owner]=[0]*6
        if denied_witness: mana[actor] = [0]*6
        # Source payment controls verify both real source payment and its main
        # trigger after restoring enough pools for the separate witness spell.
        pre = [step('turn', player=actor, value=0)]
        for who in (0, 1): pre += [step('set_mana', player=who, color=c, value=n) for c,n in enumerate(mana[who])]
        actions = []; pending_cut = None

        def receipts():
            seq = [step('assert_stack', value=0), step('assert_choices_consumed')]
            for ref, e in entities.items():
                if e['alive']:
                    seq += [observe(ref, 'in_play', 1), observe(ref, 'in_hand', 0), observe(ref, 'tapped', int(e['tapped']))]
                    if e['creature']:
                        seq += [observe(ref, 'power', e['power']), observe(ref, 'toughness', e['toughness']), observe(ref, 'damage', 0)]
                        if p.get('mechanism'):seq += [observe(ref,'plus1_counters',e['counters']),observe(ref,'minus1_counters',0)]
                elif e['hand']: seq.append(observe(ref, 'in_hand', 1))
            universe = set(HAND + GRAVE + EXILE + LIBRARY) | {name, witness, 'Counterspell', 'Hill Giant', 'Sol Ring'}
            for who in (0, 1):
                seq += [step('assert_life', player=who, value=life[who]),
                        step('assert_zone', player=who, zone='battlefield', value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                for z in zones:
                    ns = zones[z][who]; seq.append(step('assert_zone', player=who, zone=z, value=len(ns)))
                    seq += [step('assert_zone_multiplicity', player=who, zone=z, index=rows[n]['csvid'], value=ns.count(n)) for n in sorted(universe)]
                    if z == 'library': seq += [step('assert_zone_card', player=who, zone=z, index=i, value=rows[n]['csvid']) for i,n in enumerate(ns)]
                seq.append(step('assert_mana_total', player=who, value=sum(mana[who])))
                if mode in ('trigger_payment','trigger_insufficient_mana','source_insufficient_mana','source_wrong_color','trigger_unaffordable'):
                    seq += [step('assert_mana', player=who, color=c, value=n) for c,n in enumerate(mana[who])]
            return seq

        def spend(who, cost):
            mana[who] = [a-b for a,b in zip(mana[who], cost)]

        def cast_watcher():
            actions.extend([step('turn', player=owner, value=0), step('cast', ref='source', value=int(not denied_source))])
            if denied_source:
                actions.extend([step('assert_choices_consumed'), step('assert_stack', value=0)]); return
            spend(owner, source_cost); zones['hand'][owner].remove(name)
            entities['source'].update(hand=False, alive=True)
            actions.extend([step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')])
            if mode == 'source_payment':
                actions.extend(step('assert_mana', player=owner, color=c, value=n) for c,n in enumerate(mana[owner]))
                mana[owner] = [20]*6
                actions.extend(step('set_mana', player=owner, color=c, value=n) for c,n in enumerate(mana[owner]))

        def trigger(nm, who, target_ref=None):
            if not matches(oracle[nm], p['predicate']): return []
            seq = []
            for ref in ('source', 'copy'):
                if ref not in entities or not entities[ref]['alive']: continue
                if p['scope'] == 'own' and who != owner or p['scope'] == 'opponent' and who == owner: continue
                if p['predicate']=='heroic' and target_ref!=ref:continue
                cost=p.get('trigger_mana',[0]*6)
                affordable=all(mana[owner][c]>=cost[c] for c in range(1,6)) and sum(mana[owner])-sum(cost[1:])>=cost[0]
                if p['optional'] and owner == 0:
                    if mode in ('decline','alternate_decline') or not affordable: continue
                    if not payment_ui:seq.append(step('choose_trigger', player=owner, ref=ref))
                if not affordable:continue
                if p.get('trigger_cost'):spend(owner,cost)
                target = owner if p['recipient'] == 'source' else who
                amount = p['amount']
                if p['effect'] == 'gain': life[target] += amount
                elif p['effect'] == 'lose': life[target] -= amount
                elif p['effect'] in ('draw', 'mill'):
                    if p['effect'] == 'mill': amount = min(amount, len(zones['library'][target]))
                    if amount > len(zones['library'][target]): raise ValueError('draw fixture must have enough cards')
                    zone = 'hand' if p['effect'] == 'draw' else 'graveyard'
                    zones[zone][target] += zones['library'][target][:amount]; del zones['library'][target][:amount]
                elif p['effect'] in ('pump','counters'):
                    e=entities[ref]
                    if p['effect']=='counters':
                        e['counters']+=amount;e['power']+=amount;e['toughness']+=amount
                    else:e['power']+=amount[0];e['toughness']+=amount[1]
                    if e['toughness']<=0:
                        e['alive']=False;zones['graveyard'][e['player']].append(e['name'])
            return seq

        if cast_source and mode != 'source_arrives_late': cast_watcher()
        if mode == 'source_removed_before':
            actions.append(step('destroy', ref='source')); entities['source']['alive'] = False; zones['graveyard'][owner].append(name)
        if not denied_source:
            for i in range(count):
                ref = f'spell_{i}'; e = entities[ref]
                actions.append(step('turn', player=actor, value=0))
                target_ref='source' if p['predicate']=='heroic' and mode not in ('other_target','source_removed_before','source_arrives_late') else 'idle'
                if witness in ('Giant Growth','Brute Force') and not denied_witness: actions.append(step('choose_target', player=actor, ref=target_ref))
                actions.append(step('cast', ref=ref, value=int(not denied_witness)))
                if denied_witness:
                    actions.extend([step('assert_stack', value=0), step('assert_choices_consumed')]); continue
                spend(actor, witness_cost); e.update(hand=False, pending=True)
                actions += [step('assert_stack', value=1), observe(ref, 'in_hand', 0)]
                actions += trigger(witness, actor,target_ref)
                if mode in ('countered_trigger', 'source_removed_after_cast'):
                    actions.append(step('announce_spell', ref=ref))
                    if offer_probe:actions.append(step('assert_unused_choices',value=0))
                if pending_cut is None: pending_cut = len(actions)
                if mode == 'source_removed_after_cast':
                    actions.append(step('destroy', ref='source')); entities['source']['alive']=False; zones['graveyard'][owner].append(name)
                if mode == 'countered_trigger':
                    response_actor = 1-actor
                    actions.append(step('cast_response', ref='response', value=1)); spend(response_actor, cost_from_oracle(oracle['Counterspell']))
                    actions += [step('assert_stack', value=2)] + trigger('Counterspell', response_actor)
                    actions += [step('resolve_response'), step('assert_choices_consumed')]
                    zones['hand'][response_actor].remove('Counterspell'); zones['graveyard'][response_actor].append('Counterspell')
                    entities['response'].update(hand=False, pending=False)
                    zones['graveyard'][actor].append(witness)
                else:
                    if payment_decline:actions.append(step('choose_cancel_mana',player=owner))
                    actions.append(step('resolve'))
                    # Pin the optional artifact watcher's expected input
                    # consumption as a rules-facing offer assertion, then
                    # drain any mismatched plan before later actions/checks.
                    if offer_probe:actions.append(step('assert_unused_choices',value=0))
                    if payment_decline:actions.append(step('assert_unused_choices',value=0))
                    actions.append(step('assert_choices_consumed'))
                    permanent = any(t in oracle[witness]['type_line'].partition(' — ')[0].split() for t in ('Creature', 'Artifact', 'Enchantment'))
                    if permanent: e['alive']=True
                    else:
                        zones['graveyard'][actor].append(witness)
                        if witness in ('Giant Growth','Brute Force') and entities[target_ref]['alive']:
                            entities[target_ref]['power']+=3;entities[target_ref]['toughness']+=3
                        elif witness == 'Divination':
                            zones['hand'][actor] += zones['library'][actor][:2]; del zones['library'][actor][:2]
                zones['hand'][actor].remove(witness); e['pending']=False
                actions += receipts()
            if mode == 'source_arrives_late': cast_watcher()
        if mode == 'cleanup':
            # The cleanup operation exercises effects/turn cleanup, but does
            # not advance through a phase boundary that empties floated mana.
            # Keep the real payment receipt before explicitly emptying pools.
            actions += [step('assert_mana_total', player=who, value=sum(mana[who])) for who in (0,1)]
            actions += [step('set_mana', player=who, color=color, value=0) for who in (0,1) for color in range(6)]
            mana = [[0]*6, [0]*6]
            actions.append(step('cleanup'))
            for e in entities.values():
                if e['creature'] and e['alive']:
                    e['power']=int(oracle[e['name']]['power'])+(e['counters'] if p.get('mechanism') else 0)
                    e['toughness']=int(oracle[e['name']]['toughness'])+(e['counters'] if p.get('mechanism') else 0)
        checks = receipts()
        seq = pre + actions + checks
        if mode in ('ready_snapshot', 'pending_snapshot'):
            cut = 0 if mode == 'ready_snapshot' else pending_cut
            seq = pre + actions[:cut] + [step('capture')] + actions[cut:] + checks + [step('record_state'),step('ai_roundtrip'),step('restore')] + actions[cut:] + checks + [step('assert_recorded_state')]
        if mode == 'resolved_snapshot': seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        gaps = ['Multiplayer, replacement effects, unrelated trigger ordering, and keyword combat.']
        if p['optional']: gaps.append('Optional decline under the ordinary AI policy; human decline is exercised.')
        if mode == 'cleanup': gaps.append('Mana-pool expiry at phase/step boundaries is outside the cleanup helper; paid pools are checked, then explicitly emptied before value-persistence cleanup.')
        result.append(dict(name=f'CastValueTriggerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123, setup=setup, steps=seq,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                                        untested_components=gaps)))
    return result
