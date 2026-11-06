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
    if isinstance(d.get('params'),dict) and d['params'].get('schema')==6:
        return wave6_spec(d,r)
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
    if p.get('wave6'):return wave6_variants(p)
    candidates=palette(p)
    if p['destination'] in ('battlefield','battlefield_tapped'):candidates=[n for n in candidates if 'Aura' not in PINS.get(n,{}).get('type_line','') and not re.search(r'\b(?:When|Whenever)\b| enters ',PINS.get(n,{}).get('oracle_text',''))]
    return search_variants(p,candidates)


def validate_witnesses(o,rows,p):
    if p.get('wave6'):return wave6_validate_witnesses(o,rows,p)
    land_witnesses(o,rows)
    for name in palette(p):
        if name not in rows:raise ValueError('missing registered tutor witness: '+name)
        if name in PINS and any(o.get(name,{}).get(k)!=v for k,v in PINS[name].items()):raise ValueError('tutor witness pin changed: '+name)
    for name,fields in RESPONSES.items():
        if name not in rows or tuple(o[name].get(k) for k in ('mana_cost','oracle_text'))!=fields:raise ValueError('tutor reentry witness changed: '+name)


def expand(name,csvid,d,p,selected,rows,o):
    if p.get('wave6'):return wave6_expand(name,csvid,d,p,selected,rows,o)
    all_names=palette(p);choices=all_names
    if p['destination'] in ('battlefield','battlefield_tapped'):choices=[n for n in choices if 'Aura' not in o[n]['type_line'] and not re.search(r'\b(?:When|Whenever)\b| enters ',o[n].get('oracle_text',''))]
    deck=list(dict.fromkeys(['Giant Growth','Grizzly Bears']+all_names))+['Grizzly Bears']
    def observed(r,p,target):
        if d.get('gate_menu'):
            gate=target is not None and 'Gate' in o[target]['type_line'].split(' — ')[-1].split()
            return 'Gate' in r['type_line'].split(' — ')[-1].split() if gate else {'Basic','Land'}<=set(r['type_line'].split(' — ')[0].split())
        return eligible(r,p)
    return search_expand(name,csvid,d,p,selected,rows,o,palette=choices,base_deck=deck,match=eligible,observed_match=observed)


# This opt-in branch has its own pins and per-profile palettes.  In particular,
# adding a new witness never changes the established single-card declarations.
def wave6_data():
    base=Path(__file__).parent/'data'
    return (json.loads((base/'library_search_wave6_profiles.json').read_text(encoding='utf8')),
            json.loads((base/'library_search_wave6_witnesses.json').read_text(encoding='utf8')))


def wave6_exact(actual,pinned):
    """JSON pin equality includes type: a boolean is never an integer pin."""
    if type(actual) is not type(pinned):return False
    if isinstance(pinned,dict):return set(actual)==set(pinned) and all(wave6_exact(actual[k],v) for k,v in pinned.items())
    if isinstance(pinned,list):return len(actual)==len(pinned) and all(wave6_exact(a,b) for a,b in zip(actual,pinned))
    return actual==pinned


def wave6_spec(d,r):
    from recipes import cost_from_oracle
    profiles,_=wave6_data();p=d['params'];entry=profiles['profiles'].get(p.get('profile'))
    if (not entry or not wave6_exact(p,entry['params']) or not r or r.get('layout')!='normal' or
            r.get('card_faces') or r.get('selected_face') or 'variants' not in d or
            d.get('oracle_text')!=r.get('oracle_text') or
            any(not wave6_exact(r.get(k),v) for k,v in entry['source'].items())):
        raise ValueError('wave6 library search requires its complete reviewed profile and whole Oracle pin')
    return dict(p,wave6=True,family='library_search',source_name=r['name'],
                creature='Creature' in r['type_line'].partition(' — ')[0].split(),
                legendary='Legendary' in r['type_line'].partition(' — ')[0].split(),source_keywords=list(r.get('keywords',[])),
                spell_mana=cost_from_oracle(r),cost=cost_from_oracle(dict(mana_cost=p['activation_cost'])) if p['activation_cost'] else [0]*6)


def wave6_match(r,p):
    if p.get('groups'):return any(wave6_match(r,g) for g in p['groups'])
    base,_,sub=r['type_line'].partition(' — ');types=set(base.split())
    scope=p['scope']
    matches=(scope=='card' or scope=='creature' and 'Creature' in types or
            scope=='enchantment' and 'Enchantment' in types or
            scope=='artifact' and 'Artifact' in types or
            scope=='instant' and 'Instant' in types or
            scope=='planeswalker' and 'Planeswalker' in types or
            scope=='equipment' and 'Equipment' in sub.split() or
            scope=='subtype_card' and (p['subtype'] in sub.split() or 'Changeling' in r.get('keywords',[])) or
            scope=='subtype_creature' and 'Creature' in types and (p['subtype'] in sub.split() or 'Changeling' in r.get('keywords',[])) or
            scope=='keyword_creature' and 'Creature' in types and bool({'Deathtouch','Hexproof','Reach','Trample'}&set(r.get('keywords',[]))) or
            scope=='color_card' and p['color'] in r.get('colors',[]) or
            scope=='land_subtype' and 'Land' in types and p['subtype'] in sub.split() or
            scope=='forest' and 'Land' in types and 'Forest' in sub.split() or
            scope=='basic_forest' and {'Basic','Land'}<=types and 'Forest' in sub.split() or
            scope=='creature_or_land' and bool({'Creature','Land'}&types) or
            scope=='black_card' and 'B' in r.get('colors',[]) or
            scope=='basic_land' and {'Basic','Land'}<=types or
            scope=='basic_or_gate' and ({'Basic','Land'}<=types or 'Gate' in sub.split()))
    return matches and (p.get('limit_value') is None or r.get('cmc',0)<=p['limit_value']) and (p.get('exact_value') is None or r.get('cmc',0)==p['exact_value'])


def wave6_group_choices(p,remaining,v,owner,rows,o):
    from recipes import step
    chosen=[];actions=[];inspected=None;last_predicate=None
    for index,group in enumerate(p['groups']):
        legal=[n for n in remaining if wave6_match(o[n],group)]
        if v in ('last_choice','alternate_categories'):legal.reverse()
        preferred=group.get('alternate') if v=='alternate_categories' else group['preferred'] if v!='last_choice' else None
        if preferred in legal:legal.remove(preferred);legal.insert(0,preferred)
        wanted=not (v=='decline' or v=='one_selected' and index>0 or v=='two_selected' and index>1 or v=='skip_first' and index==0)
        n=legal[0] if wanted and legal else None
        rejected=p.get('invalid_last') if v=='invalid_last_category' and index==len(p['groups'])-1 else None
        if rejected:n=None
        if remaining:inspected=list(remaining);last_predicate=group
        if n:
            actions.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']));remaining.remove(n);chosen.append(n)
        else:actions.append(step('choose_zone_card' if rejected else 'choose_optional_zone',player=owner,value=rows[rejected or group['preferred']]['csvid']))
    return chosen,remaining,inspected,last_predicate,actions


def wave6_grave(p,v,bare=False):
    if bare or v=='opposing_context':return []
    if p.get('quantity_mode')=='threshold' and v.startswith('threshold'):
        return list(p['grave_pool'][:int(re.match(r'threshold(\d)',v)[1])])
    if p.get('quantity_mode')=='mastery' and v.startswith('mastery'):
        return list(p['mastery_spells'][:int(re.match(r'mastery(\d)',v)[1])])+['Scathe Zombies','Mountain']
    if p.get('condition')=='delirium':
        if v=='delirium4':return ['Scathe Zombies','Mountain','Sol Ring','Giant Growth']
        if v=='artifact_creature_types':return ['Phyrexian Hulk','Mountain','Giant Growth']
    if p.get('condition')=='undergrowth':
        if v.startswith('creatures'):return list(p['creature_grave_pool'][:int(v[-1])])+['Mountain']
        if v=='limit_boundary':return list(p['creature_grave_pool'][:2])+['Mountain']
    return list(p.get('initial_graveyard',['Scathe Zombies','Mountain']))


def wave6_context(p,grave,o):
    p=dict(p)
    if p.get('quantity_mode')=='threshold':p['quantity']=3 if len(grave)>=7 else 1
    if p.get('quantity_mode')=='mastery':p['quantity']=3 if sum(bool({'Instant','Sorcery'}&set(o[n]['type_line'].partition(' — ')[0].split())) for n in grave)>=2 else 2
    if p.get('condition')=='delirium':
        card_types={'Artifact','Battle','Creature','Enchantment','Instant','Kindred','Land','Planeswalker','Sorcery'}
        types=set().union(*(set(o[n]['type_line'].partition(' — ')[0].split())&card_types for n in grave))
        p['scope']='creature_or_land' if len(types)>=4 else 'basic_land'
    if p.get('condition')=='undergrowth':
        p['limit_value']=sum('Creature' in o[n]['type_line'].partition(' — ')[0].split() for n in grave)
    return p


def wave6_variants(p):
    if p['mode']=='entry':return wave6_entry_variants(p)
    modes=['search','payment','empty_library','single_available','duplicates','last_choice',
           'bare_zones','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot']
    if p['mode']=='spell':modes+=['insufficient_mana','wrong_color','countered']
    else:
        modes+=['insufficient_mana','opposing_turn','tax']
        if not p['sacrifice']:modes+=['source_destroyed_pending']
        if p['tap']:modes+=['tapped','untap']
        if not (p['creature'] and p['tap']) and (p['sacrifice'] or p.get('cost_interface')=='typed'):modes+=['source_cast']
        if p['creature'] and p['tap']:modes+=['sick']
        if p.get('discard_cost'):modes+=['empty_hand']
        if any(p['cost'][1:]):modes+=['wrong_color']
        if p.get('cost_interface')=='typed':modes+=['wrong_discard']
        if p.get('query_zone_choice'):modes+=['query_purity']
    if p['scope']!='card' or p['up_to']:modes+=['decline','one_selected']
    if p['scope']!='card':modes+=['no_match']
    if p['kicker']:modes+=['kicked','kicked_payment','kicked_countered','kicker_unavailable']
    if p.get('kicked_quantity'):modes+=['kicked_short']
    if p['target']=='opponent':modes+=['borrowed_death','borrowed_bounce','target_shrouded_pending']
    if p['target']=='player':modes+=['own_target','target_shrouded_pending']
    if p.get('shuffle_probe'):modes+=['shuffle_trigger','shuffle_trigger_empty']
    if p.get('artifact_choices'):modes+=['ordinary_artifact','artifact_land']
    if p.get('groups'):modes+=['skip_first','alternate_categories','invalid_last_category']
    if p['scope']=='basic_or_gate':modes+=['gate_only','basics_only','wrong_land']
    if p['quantity']>2 and (p['scope']!='card' or p['up_to']):modes+=['two_selected']
    if p['quantity']==1:modes+=['duplicate_available']
    if p['destination']=='split':modes+=['single_to_grave']
    if p['scope']=='card' and not p['up_to']:modes+=['required_decline']
    if p['destination']=='split' and p['scope']=='card':modes+=['single_required_decline']
    if p.get('quantity_mode') in ('lands','tapped_creatures','power'):
        modes+=['context_zero','context_three','context_partial','opposing_context']
        if p['quantity_mode']=='power':modes+=['context_zero_power','context_max']
    if p.get('quantity_mode')=='threshold':modes+=['threshold6','threshold7','threshold8','threshold7_decline','threshold7_partial','opposing_context']
    if p.get('quantity_mode')=='mastery':modes+=['mastery1','mastery2','mastery3','mastery2_decline','mastery2_partial','opposing_context']
    if p.get('condition')=='delirium':modes+=['delirium3','delirium4','artifact_creature_types','opposing_context']
    if p.get('condition')=='undergrowth':modes+=['creatures0','creatures1','creatures2','creatures3','limit_boundary','opposing_context']
    elif p.get('limit_value') is not None:modes+=['limit_boundary']
    if p.get('condition')=='morbid':modes+=['own_creature_died','opposing_creature_died','artifact_died']
    # These are new receipt scenarios; established pilot bodies stay exact.
    if not p.get('observe_shuffle'):
        modes+=['shuffle_receipt','shuffle_empty_receipt','shuffle_pending_snapshot']
        if p['scope']!='card' or p['up_to']:modes+=['shuffle_decline_receipt']
        if p['mode']=='spell':modes+=['shuffle_countered_receipt']
    result=[m+'_'+str(owner) for owner in (0,1) for m in modes]
    if p.get('condition')=='morbid':result.append('morbid_hand_0')
    return result


def wave6_validate_witnesses(o,rows,p):
    _,w=wave6_data()
    for name in p['witness_universe']:
        pin=w['cards'].get(name)
        if not pin or name not in rows or any(not wave6_exact(o.get(name,{}).get(k),v) for k,v in pin.items()):
            raise ValueError('wave6 search witness pin changed: '+name)


def wave6_expand(name,csvid,d,p,selected,rows,o):
    if p['mode']=='entry':return wave6_entry_expand(name,csvid,d,p,selected,rows,o)
    import copy
    from collections import Counter
    from recipes import card,step,observe,cost_from_oracle
    result=[];base_profile=p
    for variant in selected:
        p=dict(base_profile)
        v,who=variant.rsplit('_',1);owner=int(who);opponent=1-owner
        shuffle_receipt=p.get('observe_shuffle',False) or v.startswith('shuffle_')
        v={'shuffle_receipt':'search','shuffle_empty_receipt':'empty_library','shuffle_pending_snapshot':'pending_snapshot',
           'shuffle_decline_receipt':'decline','shuffle_countered_receipt':'countered'}.get(v,v)
        forced_expected=p['quantity'] if v=='required_decline' and p.get('decline_each_slot') else int(v in ('required_decline','single_required_decline'))
        searched=opponent if p['target'] in ('opponent','player') and v!='own_target' else owner
        spell=p['mode']=='spell';bare=v=='bare_zones'
        dynamic=bool(p.get('quantity_mode') or p.get('condition'))
        initial_grave=wave6_grave(p,v,bare) if dynamic else None
        if dynamic:p=wave6_context(p,initial_grave,o)
        zones={z:[[],[]] for z in ('hand','graveyard','exile','library')}
        setup=[];board=[];mana=[[30]*6,[30]*6];life=[20,20];energy=[0,0];top=[]
        cost=list(p['spell_mana'] if spell else p['cost'])
        kicked=bool(p['kicker'] and v in ('kicked','kicked_payment','kicked_countered','kicked_short'))
        if kicked:cost[0]+=p['kicker']
        count=p.get('kicked_quantity',p['quantity']+1) if kicked else p['quantity']
        zones['library'][searched]=list(p['library'])
        zones['library'][1-searched]=['Scathe Zombies','Mountain','Lightning Bolt','Seal of Cleansing']
        available=[n for n in zones['library'][searched] if wave6_match(o[n],p)]
        if owner==1 and p.get('ai_own_turn_preferred') and v!='opposing_turn':
            n=p['ai_own_turn_preferred']
            if n in available:available.remove(n);available.insert(0,n)
        if v in ('empty_library','shuffle_trigger_empty'):zones['library'][searched]=[]
        if v=='kicked_short':zones['library'][searched]=zones['library'][searched][:5]
        if v in ('single_available','single_to_grave','single_required_decline'):zones['library'][searched]=available[:1]
        if v=='duplicates':zones['library'][searched]=[available[0]]*count+['Lightning Bolt']
        if v=='duplicate_available':zones['library'][searched]=[available[0],available[0],'Lightning Bolt']
        if v in ('no_match','wrong_land'):zones['library'][searched]=[n for n in p['library'] if not wave6_match(o[n],p)]
        if v=='gate_only':zones['library'][searched]=['Azorius Guildgate','Darksteel Citadel','Giant Growth']
        if v=='basics_only':zones['library'][searched]=['Forest','Island','Darksteel Citadel','Giant Growth']
        if not bare:
            for player in (0,1):
                zones['hand'][player]=['Divination','Giant Growth']
                zones['graveyard'][player]=['Scathe Zombies','Mountain']
                zones['exile'][player]=['Lightning Bolt','Sol Ring']
        if dynamic:
            zones['graveyard'][owner]=list(initial_grave)
            if v=='opposing_context':zones['graveyard'][opponent]=list(p.get('grave_pool',p.get('initial_graveyard',[])))
        # Generic activation costs let the AI choose its discard directly,
        # without the human's policy-controlled zone selector.  Identical
        # offered copies keep its legal autonomous choice identity-independent.
        if p.get('discard_cost') and owner==1 and not bare:zones['hand'][owner]=['Giant Growth','Giant Growth']
        if p.get('cost_interface')=='typed':zones['hand'][owner]=[p['cost_witness'],p['cost_witness']]+(['Divination'] if owner==0 else [])
        if v=='empty_hand':zones['hand'][owner]=[]
        if v=='wrong_discard':zones['hand'][owner]=['Divination','Giant Growth']
        if bare and p.get('discard_cost'):zones['hand'][owner]=[p['cost_witness']] if p.get('cost_interface')=='typed' else ['Giant Growth']
        for player in (0,1):
            for zone in zones:
                setup.extend(card(n,f'p{player}_{zone}_{i}',player,zone) for i,n in enumerate(zones[zone][player]))
        def add(n,ref,player,zone='battlefield',state=None,original_owner=None):
            item=card(n,ref,player,zone)
            if state:item['state']=state
            setup.append(item)
            if zone=='battlefield':board.append(dict(name=n,ref=ref,player=player,owner=player if original_owner is None else original_owner,alive=True,tapped=bool(state and 'tapped' in state),sick=False))
            else:zones[zone][player].append(n)
        add(name,'source',owner,'hand' if spell or v=='source_cast' else 'battlefield',['tapped'] if v in ('tapped','untap') else ['sick'] if v=='sick' else None)
        for player,names in ((owner,['Grizzly Bears','Sol Ring','Forest']),(opponent,['Hill Giant','Darksteel Relic','Mountain'])):
            for i,n in enumerate(names):
                mode=p.get('quantity_mode')
                if player==owner and v in ('context_zero','opposing_context') and (mode=='lands' and n=='Forest' or mode=='power' and n=='Grizzly Bears'):continue
                if player==owner and n=='Grizzly Bears' and mode=='power':
                    if v=='context_zero_power':n='Ornithopter'
                    elif v in ('context_three','context_partial'):n='Hill Giant'
                isolated=v in ('payment','kicked_payment','kicker_unavailable','insufficient_mana','wrong_color')
                state=['tapped'] if isolated and player==owner and n in ('Sol Ring','Forest') else None
                if player==owner and n=='Grizzly Bears' and mode=='tapped_creatures' and v not in ('context_zero','opposing_context'):state=['tapped']
                add(n,f'control_{player}_{i}',player,state=state)
        if p.get('quantity_mode') in ('lands','tapped_creatures') and v in ('context_three','context_partial'):
            n='Forest' if p['quantity_mode']=='lands' else 'Grizzly Bears'
            for i in range(2):add(n,'extra_counted_'+str(i),owner,state=['tapped'] if n=='Grizzly Bears' else None)
        if p.get('quantity_mode')=='tapped_creatures':add('Craw Wurm','ready_creature_control',owner)
        if p.get('quantity_mode')=='power' and v=='context_max':add('Craw Wurm','greatest_power',owner)
        if p.get('quantity_mode') in ('lands','tapped_creatures','power') and v=='opposing_context':
            n={'lands':'Mountain','tapped_creatures':'Hill Giant','power':'Craw Wurm'}[p['quantity_mode']]
            for i in range(3):add(n,'opposing_counted_'+str(i),opponent,state=['tapped'] if p['quantity_mode']=='tapped_creatures' else None)
        if v=='tax':add('Suppression Field','tax',opponent);cost[0]+=2
        if v=='untap':add('Twiddle','untapper',owner,'hand')
        countered=v in ('countered','kicked_countered')
        if countered:add('Counterspell','counter',opponent,'hand')
        if v=='borrowed_bounce':add('Unsummon','bouncer',owner,'hand')
        if v=='target_shrouded_pending':add('Gilded Light','shield',opponent,'hand')
        if v in ('shuffle_trigger','shuffle_trigger_empty'):add('Psychogenic Probe','shuffle_probe',owner)
        if v in ('payment','kicked_payment'):mana[owner]=list(cost)
        if v=='kicker_unavailable':mana[owner]=list(p['spell_mana'])
        denied=v in ('insufficient_mana','wrong_color','tapped','sick','empty_hand','wrong_discard')
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v=='wrong_color':
            mana[owner]=list(cost);color=next(c for c in range(1,6) if cost[c]);mana[owner][1 if color!=1 else 2]+=mana[owner][color];mana[owner][color]=0
        pre=[step('turn',player=opponent if v=='opposing_turn' else owner,value=0),step('observe_search_filters',value=1)]
        if p.get('condition')=='morbid':pre.insert(0,step('reset_death_history'))
        if p.get('discard_cost') and p.get('cost_interface')!='typed':pre.append(step('discard_policy',value=2 if v=='last_choice' else 1))
        if p['kicker']:pre.append(step('present_ai_dialogs',value=1))
        pre += [step('set_mana',player=player,color=color,value=n) for player in (0,1) for color,n in enumerate(mana[player])]+[step('refresh')]
        actions=[];pending_cut=None;shuffled=False;inspected=None;fetched=[]
        def pay(n,player=owner):mana[player]=[a-b for a,b in zip(mana[player],cost_from_oracle(o[n]))]
        def finish_spell(n,player):zones['hand'][player].remove(n);zones['graveyard'][player].append(n)
        def board_entity(ref):return next(e for e in board if e['ref']==ref)
        def leave(e,zone):e['alive']=False;zones[zone][e['owner']].append(e['name'])
        def checks(pending=False):
            seq=[step('assert_stack',value=1 if pending else 0),step('assert_choices_consumed')]
            if shuffle_receipt:seq += [step('assert_shuffle_count',player=player,value=int(shuffled and player==searched)) for player in (0,1)]
            if not pending:seq.append(step('assert_forced_zone_choices',value=forced_expected))
            for e in board:
                if not e['alive']:continue
                seq += [observe(e['ref'],'in_play',1),observe(e['ref'],'tapped',int(e['tapped']))]
                if 'Creature' in o[e['name']]['type_line']:
                    seq += [observe(e['ref'],'power',int(o[e['name']]['power'])),observe(e['ref'],'toughness',int(o[e['name']]['toughness'])),observe(e['ref'],'damage',0)]
                    if e.get('fetched'):seq.append(observe(e['ref'],'sick',1))
            universe=p['witness_universe']+[name]
            for player in (0,1):
                seq += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in board))]
                if p.get('energy_gain'):seq.append(step('assert_energy',player=player,value=energy[player]))
                if v!='cleanup':seq += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
                for zone in zones:
                    ns=zones[zone][player];counts=Counter(ns)
                    seq.append(step('assert_zone',player=player,zone=zone,value=len(ns)))
                    seq += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=counts[n]) for n in universe]
                    if zone=='library' and (not shuffled or player!=searched):seq += [step('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(ns)]
            seq += [step('assert_zone_card',player=searched,zone='library',index=i,value=rows[n]['csvid']) for i,n in enumerate(top)]
            return seq
        if v=='untap':
            actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='untapper',value=1),step('choose_dialog',player=owner,value=1)]
            # Ordinary spell resolution reassesses available actions after
            # Twiddle untaps the source.  Fauna's AI legality query itself
            # presents a hand selector before the later explicit queries.
            if owner==1 and p.get('query_zone_choice'):actions.append(step('choose_zone_card',player=owner,value=rows[p['cost_witness']]['csvid']))
            actions += [step('resolve'),step('assert_choices_consumed')]
            pay('Twiddle');finish_spell('Twiddle',owner);board_entity('source')['tapped']=False
        if v=='source_cast':
            actions += [step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]
            pay(name);zones['hand'][owner].remove(name)
            board.append(dict(name=name,ref='source',player=owner,owner=owner,alive=True,tapped=False,sick=p['creature']))
            if p['creature']:actions.append(observe('source','sick',1))
        morbid=False
        if p.get('condition')=='morbid' and v in ('own_creature_died','opposing_creature_died','artifact_died','morbid_hand'):
            player=opponent if v=='opposing_creature_died' else owner
            ref=f'control_{player}_{1 if v=="artifact_died" else 0}'
            e=board_entity(ref);actions.append(step('destroy',ref=ref));leave(e,'graveyard');morbid=v!='artifact_died'
        if denied:
            if not spell:actions.append(step('require_can_activate',ref='source',value=0))
            actions += [step('cast' if spell else 'activate',ref='source',value=0),step('assert_choices_consumed')]
        else:
            # GS_CAN_ONLY_TARGET_OPPONENT commits the sole opponent directly;
            # it does not present a player-selection dialog.
            if p['target']=='player':actions.append(step('choose_player',player=owner,value=searched))
            if p['kicker'] and sum(mana[owner])-sum(p['spell_mana'])>=p['kicker']:
                actions.append(step('choose_dialog',player=owner,value=0 if kicked else 1))
            if not spell:
                for _ in range(3 if v=='query_purity' else 1):
                    if owner==1 and p.get('query_zone_choice'):actions.append(step('choose_zone_card',player=owner,value=rows[p['cost_witness']]['csvid']))
                    actions.append(step('require_can_activate',ref='source',value=1))
                    if v=='query_purity':actions+=checks()
                if p.get('cost_interface')=='typed':
                    if owner==1 and p.get('query_zone_choice'):actions.append(step('choose_zone_card',player=owner,value=rows[p['cost_witness']]['csvid']))
                    actions.append(step('choose_zone_card',player=owner,value=rows[p['cost_witness']]['csvid']))
            actions += [step('cast' if spell else 'activate',ref='source',value=1),step('assert_stack',value=1)]
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)]
            if not spell:
                e=board_entity('source');e['tapped']=p['tap']
                if p['sacrifice']:leave(e,'graveyard')
                if p.get('discard_cost'):
                    discarded=p['cost_witness'] if p.get('cost_interface')=='typed' else sorted(zones['hand'][owner],key=lambda n:rows[n]['csvid'],reverse=v=='last_choice')[0]
                    zones['hand'][owner].remove(discarded);zones['graveyard'][owner].append(discarded)
            actions += checks(pending=True);pending_cut=len(actions)
            if countered:
                actions += [step('cast_response',ref='counter',value=1),step('assert_stack',value=2),step('resolve_response'),step('assert_choices_consumed')]
                pay('Counterspell',opponent);finish_spell('Counterspell',opponent);finish_spell(name,owner)
            elif v=='target_shrouded_pending':
                # The committed-spell response wrappers require a spell
                # parent.  A response above an activated ability uses the
                # ordinary cast/resolve path and still proves stack size two.
                actions += [step('cast_response' if spell else 'cast',ref='shield',value=1),step('assert_stack',value=2),step('resolve_response' if spell else 'resolve'),step('assert_choices_consumed')]
                pay('Gilded Light',opponent);finish_spell('Gilded Light',opponent)
                if spell:finish_spell(name,owner)
            else:
                if v=='source_destroyed_pending' and not p['sacrifice']:
                    actions.append(step('destroy',ref='source'));leave(board_entity('source'),'graveyard')
                if p.get('condition')=='morbid' and morbid:
                    p=dict(p,destination='hand' if v=='morbid_hand' else 'battlefield')
                    if owner==0:actions.append(step('choose_dialog',player=owner,value=int(v=='morbid_hand')))
                own=[e for e in board if e['alive'] and e['player']==owner]
                if p.get('quantity_mode')=='lands':count=sum('Land' in o[e['name']]['type_line'].partition(' — ')[0].split() for e in own)
                if p.get('quantity_mode')=='tapped_creatures':count=sum('Creature' in o[e['name']]['type_line'].partition(' — ')[0].split() and e['tapped'] for e in own)
                if p.get('quantity_mode')=='power':count=max([0]+[int(o[e['name']]['power']) for e in own if 'Creature' in o[e['name']]['type_line'].partition(' — ')[0].split()])
                remaining=list(zones['library'][searched]);legal=[n for n in remaining if wave6_match(o[n],p)]
                inspection_predicate=p
                desired=min(count,0 if v=='decline' or v.endswith('_decline') and not forced_expected else 1 if v=='one_selected' else 2 if v=='two_selected' or v.endswith('_partial') else count)
                if v=='last_choice':legal.reverse()
                if v=='limit_boundary':legal.sort(key=lambda n:o[n]['cmc']!=p['limit_value'])
                if v in ('ordinary_artifact','artifact_land'):
                    n='Sol Ring' if v=='ordinary_artifact' else 'Darksteel Citadel'
                    legal.remove(n);legal.insert(0,n)
                if owner==1 and p.get('ai_own_turn_preferred') and v!='opposing_turn':
                    preferred=p.get('ai_own_turn_alternate',p['ai_own_turn_preferred']) if v=='last_choice' else p['ai_own_turn_preferred']
                    legal=[preferred] if preferred in legal else []
                if p.get('groups'):
                    chosen,remaining,inspected,inspection_predicate,inputs=wave6_group_choices(p,remaining,v,owner,rows,o);actions+=inputs
                else:
                    chosen=legal[:desired]
                    if v in ('single_to_grave','single_required_decline'):actions.append(step('choose_optional_zone',player=owner,value=rows[chosen[0]]['csvid']))
                    for i,n in enumerate(chosen):
                        inspected=list(remaining)
                        # Required selectors must use their legal fallback,
                        # rather than leaving findable cards unassigned.
                        actions.append(step('choose_optional_zone' if forced_expected and (i==0 or p.get('decline_each_slot')) else 'choose_zone_card',player=owner,value=rows[n]['csvid']))
                        remaining.remove(n)
                    if len(chosen)<count and v not in ('single_to_grave','single_required_decline') and (p['scope']!='card' or p['up_to']):
                        if remaining:inspected=list(remaining)
                        fallback=(next((n for n in remaining if wave6_match(o[n],p)),None) or p['preferred'][0])
                        actions += [step('choose_optional_zone',player=owner,value=rows[fallback]['csvid']) for _ in range(count-len(chosen) if p.get('sequential') else 1)]
                actions += [step('resolve'),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=forced_expected),step('refresh')]
                zones['library'][searched]=remaining;shuffled=True
                if spell:finish_spell(name,owner)
                life[owner]+=p.get('life_gain',0);energy[owner]+=p.get('energy_gain',0)
                if v in ('shuffle_trigger','shuffle_trigger_empty'):life[searched]-=2
                for i,n in enumerate(chosen):
                    destination=p['destination']
                    if destination=='split':destination='graveyard' if v in ('single_to_grave','single_required_decline') or i else 'hand'
                    if destination=='first_battlefield_tapped_rest_hand':destination='battlefield_tapped' if i==0 else 'hand'
                    if destination=='top':continue
                    if destination in ('hand','graveyard','exile'):zones[destination][searched if p.get('destination_player')=='searched' else owner].append(n)
                    else:
                        ref='found_'+str(i);copy_index=sum(e['alive'] and e['player']==owner and e['name']==n for e in board)
                        e=dict(name=n,ref=ref,player=owner,owner=searched,alive=True,tapped=destination=='battlefield_tapped',sick=True,fetched=True)
                        board.append(e);fetched.append(e)
                        actions.append(step('bind_permanent_copy',player=owner,value=rows[n]['csvid'],index=copy_index,ref=ref))
                if p['destination']=='top':
                    top=list(reversed(chosen));zones['library'][searched][:0]=top
                if inspected:
                    actions.append(step('assert_offered_count',value=len(inspected)))
                    # With only one available card, this first hand selector
                    # may be declined to assign that card to the graveyard.
                    # Its local optional flag alone cannot violate the spell.
                    if not (p['destination']=='split' and p['scope']=='card' and v=='single_available'):
                        actions.append(step('assert_offered_selection_required',value=int(p['scope']=='card' and not p['up_to'])))
                    if not (owner==1 and p.get('ai_own_turn_preferred') and v!='opposing_turn'):
                        actions += [step('assert_offered_legal_card',index=rows[n]['csvid'],value=int(wave6_match(o[n],inspection_predicate))) for n in sorted(set(inspected))]
                if v=='borrowed_death' and fetched:actions.append(step('destroy',ref=fetched[0]['ref']));leave(fetched[0],'graveyard')
                if v=='borrowed_bounce' and fetched:
                    actions += [step('choose_target',player=owner,ref=fetched[0]['ref']),step('cast',ref='bouncer',value=1),step('resolve'),step('assert_choices_consumed')]
                    pay('Unsummon');finish_spell('Unsummon',owner);leave(fetched[0],'hand')
        final=checks()
        if v=='cleanup':actions += [step('cleanup'),step('refresh')]
        if v in ('ready_snapshot','pending_snapshot'):
            cut=0 if v=='ready_snapshot' else pending_cut
            tail=actions[cut:]+final
            seq=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif v=='resolved_snapshot':seq=pre+actions+final+[step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        else:seq=pre+actions+final
        gaps=list(p['gaps'])
        if shuffle_receipt:gaps=[g for g in gaps if not g.startswith('shuffle invocation/count')]+['shuffle random distribution']
        if v=='source_cast':gaps=[g for g in gaps if not g.startswith('casting the source permanent')]
        result.append(dict(name=f'LibrarySearchRecipe_{csvid}_{variant}',card=name,fixture='empty_main',setup=setup,steps=seq,seed=123,
                           _recipe=dict(family='library_search',version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
    return result


def wave6_entry_params(p):
    return dict(p,optional=p['entry_optional'],pay_life=0,lose_life=0,enters_tapped=False,mana_ability=False,
                snow=False,types=[],basic=False,failure_allowed=True,other_free_ability=False,
                keywords=p['source_keywords'],untested_rules=p['other_rules'])


def wave6_entry_variants(p):
    q=wave6_entry_params(p)
    return [v for v in search_variants(q,p['palette']) if v.rsplit('_',1)[0] not in p['exclude_variants']]


def wave6_entry_expand(name,csvid,d,p,selected,rows,o):
    from recipes import step
    q=wave6_entry_params(p)
    presentation=dict(d,trigger_priority=p['entry_priority'],trigger_players=[0] if p['entry_priority'] else [],
                      entry_dialogs=p['entry_dialogs'],decline_entry_dialogs=p['decline_entry_dialogs'])
    cases=search_expand(name,csvid,presentation,q,selected,rows,o,palette=p['palette'],base_deck=p['library'],
                        match=wave6_match,base_zones=p['base_zones'],attempt_rejected=True)
    for case in cases:
        variant=case['_recipe']['variant'];mode,owner=variant.rsplit('_',1);owner=int(owner)
        denied=mode in ('insufficient_mana','wrong_color','countered')
        skipped=mode=='decline' and (p['entry_priority'] and owner==0 or p['decline_skips_search'])
        shuffles=0 if denied or skipped else 2 if mode in ('two_sources','bounce_recast','blink') else 1
        # Added only to this opt-in cohort.  The passive counter follows both
        # snapshot and AI rollback and does not constrain the permutation.
        case['steps'][:0]=[step('assert_shuffle_count',player=pl,value=0) for pl in (0,1)]
        case['steps'] += [step('assert_shuffle_count',player=pl,value=shuffles if pl==owner else 0) for pl in (0,1)]
        case['_recipe']['untested_components']=p['gaps']
    return cases
