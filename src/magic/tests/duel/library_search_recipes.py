"""Reviewed one-card library tutors with independently pinned search witnesses."""
import json
import re
from pathlib import Path
from creature_recipes import oracle_keywords
from land_search_recipes import LANDS,variants as search_variants,validate_witnesses as land_witnesses,expand as search_expand

PINS=json.loads((Path(__file__).parent/'data/library_search_witnesses.json').read_text(encoding='utf8'))['cards']
PERMANENTS={'Artifact','Creature','Enchantment','Land','Planeswalker','Battle'}
SCOPES={
    'card':('',(),(),(),None,None),
    'creature':('creature',('Creature',),(),(),None,None),
    'artifact':('artifact',('Artifact',),(),(),None,None),
    'enchantment':('enchantment',('Enchantment',),(),(),None,None),
    'instant':('instant',('Instant',),(),(),None,None),
    'sorcery':('sorcery',('Sorcery',),(),(),None,None),
    'instant_or_sorcery':('instant or sorcery',(),('Instant','Sorcery'),(),None,None),
    'artifact_or_enchantment':('artifact or enchantment',(),('Artifact','Enchantment'),(),None,None),
    'artifact_creature':('artifact creature',('Artifact','Creature'),(),(),None,None),
    'permanent':('permanent',(),tuple(PERMANENTS),(),None,None),
    'legendary_card':('legendary',(),(),(),True,None),
    'legendary_creature':('legendary creature',('Creature',),(),(),True,None),
    'green_creature':('green creature',('Creature',),(),(),None,'G'),
    'nonlegendary_green_creature':('nonlegendary green creature',('Creature',),(),(),False,'G'),
    'blue_instant':('blue instant',('Instant',),(),(),None,'U'),
    'basic_land':('basic land',('Land','Basic'),(),(),None,None),
    'forest':('Forest',('Land',),(),('Forest',),None,None),
    'plains':('Plains',('Land',),(),('Plains',),None,None),
    'basic_or_gate':('basic land card or Gate',(),(),(),None,None),
    'aura':('Aura',(),(),('Aura',),None,None),
    'equipment':('Equipment',(),(),('Equipment',),None,None),
    'aura_or_equipment':('Aura or Equipment',(),(),('Aura','Equipment'),None,None),
    'planeswalker':('planeswalker',('Planeswalker',),(),(),None,None),
    'goblin':('Goblin',(),(),('Goblin',),None,None),
    'elf':('Elf',(),(),('Elf',),None,None),
    'faerie':('Faerie',(),(),('Faerie',),None,None),
    'giant':('Giant',(),(),('Giant',),None,None),
    'kithkin':('Kithkin',(),(),('Kithkin',),None,None),
    'merfolk':('Merfolk',(),(),('Merfolk',),None,None),
    'treefolk_or_forest':('Treefolk or Forest',(),(),('Treefolk','Forest'),None,None),
    'rebel_permanent':('Rebel permanent',(),tuple(PERMANENTS),('Rebel',),None,None),
    'mercenary_permanent':('Mercenary permanent',(),tuple(PERMANENTS),('Mercenary',),None,None),
    'mercenary':('Mercenary',(),(),('Mercenary',),None,None),
    'dragon_creature':('Dragon creature',('Creature',),(),('Dragon',),None,None),
    'arcane':('Arcane',(),(),('Arcane',),None,None),
    'trap':('Trap',(),(),('Trap',),None,None),
    'assembly_worker_creature':('Assembly-Worker creature',('Creature',),(),('Assembly-Worker',),None,None),
    'named':('',(),(),(),None,None),
}
CREATURE_SUBTYPES={'Goblin','Elf','Faerie','Giant','Kithkin','Merfolk','Treefolk','Rebel','Mercenary','Dragon','Assembly-Worker'}
CORE=['Grizzly Bears','Ornithopter','Phyrexian Hulk','Hill Giant','Sol Ring','Darksteel Relic','Seal of Cleansing','Pacifism','Bonesplitter','Forest','Darksteel Citadel','Lightning Bolt','Giant Growth','Counterspell','Divination','Isamaru, Hound of Konda','Jace Beleren']
EXTRAS={
 'goblin':["Mons's Goblin Raiders",'Tarfire','Goblin Goon'],
 'elf':['Llanowar Elves'], 'faerie':['Cloud Sprite'], 'giant':['Hill Giant'],
 'kithkin':['Kithkin Zephyrnaut'],'merfolk':['Merfolk of the Pearl Trident'],
 'treefolk_or_forest':['Oakgnarl Warrior','Taiga'],
 'rebel_permanent':['Ramosian Sergeant','Steadfast Guard','Nightwind Glider','Bound in Silence'],
 'mercenary_permanent':['Rampart Crawler','Cateran Persuader','Misshapen Fiend','Rathi Intimidator'],
 'mercenary':['Rampart Crawler','Cateran Persuader','Misshapen Fiend'],
 'dragon_creature':['Shivan Dragon','Scion of Ugin'], 'arcane':['Reach Through Mists'],
 'trap':['Whiplash Trap','Mindbreak Trap'], 'assembly_worker_creature':['Self-Assembler'],
 'aura':['Holy Strength'],'equipment':['Vulshok Morningstar'],'aura_or_equipment':['Holy Strength','Vulshok Morningstar'],
 'basic_or_gate':[*LANDS,'Azorius Guildgate'], 'basic_land':LANDS,'forest':LANDS,'plains':LANDS,
 'green_creature':['Llanowar Elves','Centaur Courser','Giant Spider','Craw Wurm','Isao, Enlightened Bushi'],
 'nonlegendary_green_creature':['Llanowar Elves','Centaur Courser','Giant Spider','Craw Wurm','Isao, Enlightened Bushi'],
 'blue_instant':['Cancel'],
}
RESPONSES={'Unsummon':('{U}',"Return target creature to its owner's hand."),
 'Cloudshift':('{W}','Exile target creature you control, then return that card to the battlefield under your control.')}


def eligible(r,p):
    _,all_types,any_types,subs,legend,color=SCOPES[p['scope']]
    base,_,sub=r['type_line'].partition(' — ');base=set(base.split());sub=set(sub.split())
    if p['scope']=='basic_or_gate':
        if not ({'Basic','Land'}<=base or 'Gate' in sub):return False
    elif not set(all_types)<=base or (any_types and not set(any_types)&base):return False
    if subs and not (set(subs)&sub or 'Changeling' in r.get('keywords',[]) and set(subs)&CREATURE_SUBTYPES):return False
    if legend is not None and ('Legendary' in base)!=legend:return False
    if color and color not in r.get('colors',[]):return False
    if p['scope']=='named' and r['name'] not in p['names']:return False
    limit=p['limit']
    if limit:
        field,op,value=limit['field'],limit['op'],limit['value'];actual=r.get('cmc') if field=='mana_value' else r.get(field)
        try:actual=float(actual)
        except (TypeError,ValueError):return False
        if not {'eq':actual==value,'le':actual<=value,'ge':actual>=value}[op]:return False
    return True


def palette(p):
    names=list(dict.fromkeys(CORE+EXTRAS.get(p['scope'],[])))
    if SCOPES[p['scope']][3]:names+=['Avian Changeling','Mothdust Changeling','Crib Swap','Nameless Inversion']
    if p['scope']=='named':names+=p['names']
    if p['limit']:
        names+=['Air Elemental','Gilded Lotus','Phyrexian Colossus','Vulshok Morningstar','Darksteel Ingot','Kraken Hatchling','Fireball','Centaur Courser','Giant Spider']
    names=list(dict.fromkeys(names))
    # Auras need attachment decisions when fetched onto the battlefield.
    # Keep them in the mixed inspection deck, but defer selecting them there.
    return [n for n in names if n in PINS or n==p['source_name']]


def spec(d,r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('library tutor requires a whole pinned normal card')
    p=d.get('params')
    required={'mode','scope','limit','destination','optional','activation_cost','tap','sacrifice','pay_life','keywords','names','lose_life'}
    if not isinstance(p,dict) or set(p)!=required:raise ValueError('explicit tutor predicate, destination, mode, costs and source keywords required')
    if p['mode'] not in ('spell','entry','activated') or not isinstance(p['scope'],str) or p['scope'] not in SCOPES or p['destination'] not in ('hand','top','battlefield','battlefield_tapped'):raise ValueError('unreviewed tutor scope or destination')
    if not isinstance(p['keywords'],list) or any(not isinstance(k,str) for k in p['keywords']) or p['keywords']!=sorted(set(p['keywords'])):raise ValueError('explicit ordered source keywords required')
    if any(type(p[k]) is not bool for k in ('optional','tap','sacrifice')) or any(type(p[k]) is not int or not 0<=p[k]<=3 for k in ('pay_life','lose_life')):raise ValueError('explicit booleans and bounded life amounts required')
    if not isinstance(p['names'],list) or any(not isinstance(n,str) for n in p['names']) or len(p['names'])!=len(set(p['names'])) or (p['scope']=='named')!=(1<=len(p['names'])<=2):raise ValueError('only named searches specify one/two exact names')
    limit=p['limit']
    if limit is not None and (not isinstance(limit,dict) or set(limit)!={'field','op','value'} or limit['field'] not in ('mana_value','power','toughness') or limit['op'] not in ('eq','le','ge') or type(limit['value']) is not int or not 0<=limit['value']<=8):raise ValueError('bounded fixed mana/power/toughness limit required')
    if not isinstance(p['activation_cost'],str) or p['activation_cost'] and not re.fullmatch(r'(?:\{(?:[1-9]|[BUGRW])\})+',p['activation_cost']):raise ValueError('ordinary activation mana required')
    noun=SCOPES[p['scope']][0];article='an' if noun.startswith(('artifact','enchantment','instant','Aura','Equipment','Elf','Arcane','Assembly')) else 'a'
    phrase=article+' '+(noun+' ' if noun else '')+'card'
    if p['scope']=='named':phrase='a card named '+' or '.join(p['names'])
    if limit:
        label={'mana_value':'mana value','power':'power','toughness':'toughness'}[limit['field']]
        phrase+=f" with {label} {limit['value']}"+{'eq':'','le':' or less','ge':' or greater'}[limit['op']]
    clauses=set()
    for ref in ('it','that card'):
        for revealed in ('it','that card'):
            if p['destination']=='hand':ending=('put '+ref+' into your hand' if p['scope']=='card' and not limit else 'reveal '+revealed+', put '+ref+' into your hand')+', then shuffle.'
            elif p['destination']=='top':ending=('' if p['scope']=='card' and not limit else 'reveal '+revealed+', ')+'then shuffle and put '+ref+' on top.'
            else:ending='put '+ref+' onto the battlefield'+(' tapped' if p['destination']=='battlefield_tapped' else '')+', then shuffle.'
            clauses.add('Search your library for '+phrase+', '+ending)
    if p['scope']=='basic_or_gate':clauses|={c.replace('or Gate card','or a Gate card') for c in clauses}
    lines=r['oracle_text'].splitlines();remaining=list(lines);kind=r['type_line'].split(' — ')[0].split();creature='Creature' in kind
    if p['mode']=='spell':expected={c+(' You lose '+str(p['lose_life'])+' life.' if p['lose_life'] else '') for c in clauses}
    elif p['mode']=='entry':
        expected={f'When {ref} enters{suffix}, '+('you may ' if p['optional'] else '')+c[0].lower()+c[1:] for ref in ('this creature',r['name'],r['name'].split(',')[0]) for suffix in ('',' the battlefield') for c in clauses}
    else:
        permanent=next((t for t in ('Creature','Artifact','Enchantment','Land') if t in kind),None)
        if not permanent:raise ValueError('activated tutor source must be a permanent')
        costs=([p['activation_cost']] if p['activation_cost'] else [])+(['{T}'] if p['tap'] else [])+([f"Pay {p['pay_life']} life"] if p['pay_life'] else [])+(['Sacrifice this '+permanent.lower()] if p['sacrifice'] else [])
        if not costs:raise ValueError('explicit nonempty activation cost required')
        expected={', '.join(costs)+': '+c for c in clauses}
    matching=[line for line in lines if line in expected]
    if len(matching)!=1:raise ValueError('parameters must match exactly one complete printed search clause')
    remaining.remove(matching[0]);abilities=[line for line in remaining if re.match(r'^(?:\{|Pay |Sacrifice |Tap |Untap |Discard )',line) and ': ' in line];passive=[line for line in remaining if line not in abilities]
    if p['mode']=='spell':
        if not set(kind)&{'Instant','Sorcery'} or remaining or p['keywords'] or r.get('keywords') or any(p[k] for k in ('optional','tap','sacrifice','pay_life','activation_cost')):raise ValueError('ordinary single-clause tutor spell required')
    else:
        if p['lose_life'] or (p['mode']=='entry' and (not creature or any(p[k] for k in ('tap','sacrifice','pay_life','activation_cost')))) or p['mode']=='activated' and p['optional']:raise ValueError('reviewed entry or activated mode costs required')
        if creature:
            keys=oracle_keywords(dict(r,oracle_text='\n'.join(passive),keywords=p['keywords']))
            if p['keywords']!=keys:raise ValueError('explicit passive source keywords must match')
        elif passive or p['keywords'] or r.get('keywords'):raise ValueError('additional passive permanent rules require dedicated models')
    priority=d.get('trigger_priority',False)
    if type(priority) is not bool or priority and p['mode']!='entry':raise ValueError('trigger presentation annotation requires entry mode')
    players=d.get('trigger_players',[0])
    if not isinstance(players,list) or any(type(n) is not int or n not in (0,1) for n in players) or len(players)!=len(set(players)) or 'trigger_players' in d and not priority:raise ValueError('trigger players require an explicit entry priority annotation')
    for key in ('activation_dialogs','entry_dialogs','decline_entry_dialogs'):
        dialogs=d.get(key,{})
        if not isinstance(dialogs,dict) or any(k not in ('0','1') or not isinstance(v,list) or len(v)>4 or any(type(n) is not int or not 0<=n<=32 for n in v) for k,v in dialogs.items()) or dialogs and p['mode']!=('activated' if key=='activation_dialogs' else 'entry'):raise ValueError('dialog annotations require reviewed source menu choices')
    if type(d.get('gate_menu',False)) is not bool or d.get('gate_menu') and (p['mode']!='entry' or p['scope']!='basic_or_gate'):raise ValueError('Gate/basic menu annotation requires that exact entry search')
    result=dict(p,family='library_search',source_name=r['name'],creature=creature,spell_mana=cost_from_oracle(r) if r.get('mana_cost') else [0]*6,cost=cost_from_oracle(dict(mana_cost=p['activation_cost'])) if p['activation_cost'] else [0]*6,
                snow=False,types=[],basic=False,enters_tapped=False,mana_ability=False,legendary='Legendary' in kind,failure_allowed=p['scope']!='card' or limit is not None,untested_rules=abilities,other_free_ability=p['mode']=='activated' and any(a.startswith('{T}:') for a in abilities))
    selected=palette(result);pool=dict(PINS,**{r['name']:r})
    if not any(eligible(pool[n],result) for n in selected):raise ValueError('reviewed tutor palette needs a matching witness')
    return result


def variants(p):
    candidates=palette(p)
    if p['destination'] in ('battlefield','battlefield_tapped'):candidates=[n for n in candidates if 'Aura' not in PINS.get(n,{}).get('type_line','') and not re.search(r'\b(?:When|Whenever)\b| enters ',PINS.get(n,{}).get('oracle_text',''))]
    return search_variants(p,candidates)


def validate_witnesses(o,rows,p):
    land_witnesses(o,rows)
    for name in palette(p):
        if name not in rows:raise ValueError('missing registered tutor witness: '+name)
        if name in PINS and any(o.get(name,{}).get(k)!=v for k,v in PINS[name].items()):raise ValueError('tutor witness pin changed: '+name)
    for name,fields in RESPONSES.items():
        if name not in rows or tuple(o[name].get(k) for k in ('mana_cost','oracle_text'))!=fields:raise ValueError('tutor reentry witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
    all_names=palette(p);choices=all_names
    if p['destination'] in ('battlefield','battlefield_tapped'):choices=[n for n in choices if 'Aura' not in o[n]['type_line'] and not re.search(r'\b(?:When|Whenever)\b| enters ',o[n].get('oracle_text',''))]
    deck=list(dict.fromkeys(['Giant Growth','Grizzly Bears']+all_names))+['Grizzly Bears']
    def observed(r,p,target):
        if d.get('gate_menu'):
            gate=target is not None and 'Gate' in o[target]['type_line'].split(' — ')[-1].split()
            return 'Gate' in r['type_line'].split(' — ')[-1].split() if gate else {'Basic','Land'}<=set(r['type_line'].split(' — ')[0].split())
        return eligible(r,p)
    return search_expand(name,csvid,d,p,selected,rows,o,palette=choices,base_deck=deck,match=eligible,observed_match=observed)
