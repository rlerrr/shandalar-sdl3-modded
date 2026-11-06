"""Fixed token instant/sorcery components with exact inactive Oracle clauses.

The existing token creation scenarios supply real casts, payment, combat,
removal, zone receipts and snapshots. This extension independently validates
whole source text and adds the separately printed life receipt where present.
"""
import copy,json,re
from pathlib import Path
import token_creation_recipes as base

COLORS=base.COLORS
SUBTYPES=base.SUBTYPES|{'Beast','Squirrel','Saproling','Zombie','Bear','Cat','Elemental','Elephant','Kithkin','Treefolk','Shaman','Snake','Merfolk','Wizard'}
KEYWORDS=base.KEYWORDS
WORDS={'a':1,'two':2,'three':3,'four':4,'five':5,'six':6,'thirteen':13}
INACTIVE={
 'Acorn Harvest':['Flashback—{1}{G}, Pay 3 life. (You may cast this card from your graveyard for its flashback cost. Then exile it.)'],
 'Beast Attack':['Flashback {2}{G}{G}{G} (You may cast this card from your graveyard for its flashback cost. Then exile it.)'],
 'Moan of the Unhallowed':['Flashback {5}{B}{B} (You may cast this card from your graveyard for its flashback cost. Then exile it.)'],
 'Scatter the Seeds':["Convoke (Your creatures can help cast this spell. Each creature you tap while casting this spell pays for {1} or one mana of that creature's color.)"],
 "Gisa's Bidding":['Madness {2}{B} (If you discard this card, discard it into exile. When you do, cast it for its madness cost or put it into your graveyard.)'],
 'Spectral Reserves':[],
}
SOURCE_KEYWORDS={'Acorn Harvest':['Flashback'],'Beast Attack':['Flashback'],'Moan of the Unhallowed':['Flashback'],'Scatter the Seeds':['Convoke'],"Gisa's Bidding":['Madness'],'Spectral Reserves':[]}


# These source records are independently reviewed pins from the authoritative
# Oracle packet. They authorize only the fixed normal-cost creation component.
_REVIEWED_SOURCES=json.loads((Path(__file__).parent/'data/token_spell_extension_witnesses.json').read_text(encoding='utf8'))['sources']
for _name,_pin in _REVIEWED_SOURCES.items():
    INACTIVE[_name]=[line for line in _pin['oracle_text'].splitlines() if not line.startswith('Create ')]
    SOURCE_KEYWORDS[_name]=_pin['keywords']


def creation(text):
    gain=0
    m=re.search(r'\. You gain ([1-9][0-9]?) life\.$',text)
    if m:gain=int(m[1]);text=text[:m.start()]+'.'
    m=re.fullmatch(r'Create (a|two|three|four|five|six|thirteen) (tapped )?([0-8])/([1-9]|1[0-2]) (white|blue|black|red|green|colorless)(?: and (white|blue|black|red|green))? ([A-Za-z ]+?) (artifact )?creature tokens?(?: with ([a-z ,]+))?\.',text)
    if not m:raise ValueError('one complete ordinary fixed token creation effect required')
    number,tapped,power,toughness,color,second,subtypes,artifact,keywords=m.groups()
    types=subtypes.split();keys=[k.capitalize() for k in re.split(r' and |, ',keywords)] if keywords else []
    if len(types)>2 or any(t not in SUBTYPES for t in types) or len(set(types))!=len(types) or any(k not in KEYWORDS for k in keys) or len(set(keys))!=len(keys):raise ValueError('unreviewed token characteristics')
    token=dict(name=' '.join(types),count=WORDS[number],power=int(power),toughness=int(toughness),colors=[COLORS[c] for c in (color,second) if c and c!='colorless'],subtypes=types,artifact=bool(artifact),keywords=keys)
    return token,gain,bool(tapped)

def spec(d,r):
    if isinstance(d.get('params'),dict) and d['params'].get('profile')=='creator_wave6':
        from token_trigger_creation_spec import wave6_spec
        return wave6_spec(d,r,'token_spell_extension')
    if isinstance(d.get("params"),dict) and d["params"].get("profile")=="death_watch":
        from token_trigger_creation_spec import death_watch_spec
        return death_watch_spec(d,r)
    if isinstance(d.get('params'),dict) and d['params'].get('profile')=='noncreature_amount':
        from token_trigger_creation_spec import noncreature_spec
        return noncreature_spec(d,r)
    from recipes import cost_from_oracle
    if not isinstance(r,dict) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not any(t in r.get('type_line','') for t in ('Instant','Sorcery')) or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:raise ValueError('whole pinned normal instant or sorcery required')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'token','inactive_clauses','gain_life','tapped','delayed_cleanup','convoke_no_creatures'}:raise ValueError('complete explicit token spell component parameters required')
    name=r['name']
    if name not in _REVIEWED_SOURCES or any(r.get(k)!=value for k,value in _REVIEWED_SOURCES[name].items()):raise ValueError('reviewed source Oracle profile changed')
    if name not in INACTIVE or p['inactive_clauses']!=INACTIVE[name] or r.get('keywords')!=SOURCE_KEYWORDS[name]:raise ValueError('unreviewed inactive source component')
    rules=[line for line in r['oracle_text'].splitlines() if line not in INACTIVE[name]]
    inactive=[line for line in r['oracle_text'].splitlines() if line in INACTIVE[name]]
    if inactive!=INACTIVE[name] or len(rules)!=1:raise ValueError('whole source text contains additional clauses')
    token,gain,tapped=creation(rules[0])
    if token!=p['token'] or gain!=p['gain_life'] or tapped!=p['tapped'] or p['delayed_cleanup'] is not None or p['convoke_no_creatures']!=('Convoke' in SOURCE_KEYWORDS[name]):raise ValueError('explicit token component disagrees with source Oracle')
    if any(type(token_value) is not int for token_value in (p['gain_life'],p['token']['count'],p['token']['power'],p['token']['toughness'])) or any(type(p[k]) is not bool for k in ('tapped','convoke_no_creatures')) or type(p['token']['artifact']) is not bool:raise ValueError('typed token component required')
    return dict(p,family='token_spell_extension',source_name=name,creature=False,keywords=[],power=0,toughness=0,mana=cost_from_oracle(r))

def variants(p):
    if p.get('_wave6_component'):
        from token_trigger_creation_spec import wave6_variants
        return wave6_variants(p)
    if p.get("_death_watch_component"):
        from token_trigger_creation_spec import death_watch_variants
        return death_watch_variants(p)
    if p.get('_noncreature_component'):
        from token_trigger_creation_spec import noncreature_variants
        return noncreature_variants(p)
    common=base.variants(p)
    if p['convoke_no_creatures'] or p['token']['count']>7 or p['mana'][0]>5 or any(k in SOURCE_KEYWORDS[p['source_name']] for k in ('Storm','Splice')):common=[v for v in common if not v.startswith('two_casts_')]
    if p['tapped']:common=[v for v in common if not v.startswith(('fresh_attack_','ready_attack_','block_ground_','block_flying_','evasion_','reach_blocker_','trample_'))]
    return common

def validate_witnesses(o,rows):
    pins=json.loads((Path(__file__).parent/'data/token_spell_extension_witnesses.json').read_text(encoding='utf8'))['cards']
    for name,pin in pins.items():
        if name not in rows or any(o.get(name,{}).get(k)!=value for k,value in pin.items()):raise ValueError('token spell witness changed: '+name)

def expand(name,csvid,d,p,selected,rows,o):
    if p.get('_wave6_component'):
        from token_trigger_creation_recipes import expand_wave6
        return expand_wave6(name,csvid,d,p,selected,rows,o)
    if p.get("_death_watch_component"):
        from token_trigger_creation_recipes import expand_death_watch
        return expand_death_watch(name,csvid,d,p,selected,rows,o)
    if p.get('_noncreature_component'):
        from token_trigger_creation_recipes import expand_noncreature_component
        return expand_noncreature_component(name,csvid,d,p,selected,rows,o)
    from recipes import step
    validate_witnesses(o,rows)
    allowed=variants(p)
    if not isinstance(selected,list) or not selected or len(set(selected))!=len(selected) or any(v not in allowed for v in selected):raise ValueError('explicit unique supported token spell variants required')
    token=p['token'];tid=rows.get(token['name'],{}).get('csvid')
    if tid is None or not o.get(token['name'],{}).get('type_line','').startswith('Token '):raise ValueError('registered pinned token identity required')
    result=base.expand(name,csvid,d,p,selected,rows,o)
    for case in result:
        v,owner=case['_recipe']['variant'].rsplit('_',1);owner=int(owner)
        casts=0 if v in ('insufficient_mana','wrong_color','countered') else 2 if v=='two_casts' else 1
        if p['convoke_no_creatures']:
            next(c for c in case['setup'] if c['ref']=='unselected')['state']=['tapped']
        steps=[];resolved=0;pending=False;saved=None
        for raw in case['steps']:
            action=copy.deepcopy(raw)
            if action['op']=='assert_life' and action['player']==owner:action['value']+=casts*p['gain_life']
            if p['convoke_no_creatures'] and action['op']=='assert_card' and action.get('ref')=='unselected' and action.get('property')=='tapped':action['value']=1
            if p['tapped'] and action['op']=='assert_card' and action.get('ref','').startswith('expected_token_') and action.get('property')=='tapped':action['value']=1
            if action['op']=='capture':saved=(resolved,pending)
            if action['op']=='restore':resolved,pending=saved
            if action['op']=='cast' and action.get('ref') in ('source','second') and action['value']==1:pending=True
            steps.append(action)
            if action['op']=='resolve' and pending:
                pending=False;resolved+=1
                # Positive creation and life receipts are observed before expiry,
                # combat, or token removal can hide a missing resolution effect.
                steps += [step('assert_zone_multiplicity',player=owner,zone='battlefield',index=tid,value=resolved*token['count']),step('assert_matching_permanents',player=owner,index=tid,property='power',value=token['power']),step('assert_matching_permanents',player=owner,index=tid,property='toughness',value=token['toughness']),step('assert_life',player=owner,value=20+resolved*p['gain_life'])]
        case['steps']=steps;case['name']=case['name'].replace('TokenCreationRecipe_','TokenSpellExtensionRecipe_')
        gaps=['inactive: '+line for line in p['inactive_clauses']]+['normal casting component only; alternate costs and graveyard casting','token doublers, copy effects, replacements, control changes and multiplayer']
        if 'Storm' in SOURCE_KEYWORDS[name]:gaps+=['storm copies after earlier spells; the source is the first spell cast this turn']
        if name=='Empyrial Storm':gaps+=['commander cast-history copies; fixtures contain no commander history']
        if 'Threshold' in SOURCE_KEYWORDS[name]:gaps+=['threshold replacement at seven or more graveyard cards; casting fixtures remain below seven']
        if 'Fateful hour' in SOURCE_KEYWORDS[name]:gaps+=['fateful-hour replacement at five or less life; source is cast at twenty life']
        if p['tapped']:gaps+=['untapping tapped tokens; this profile checks creation, persistence and removal while tapped']
        if p['convoke_no_creatures']:gaps+=['convoke discounts; all controlled creatures are tapped before the source is cast']
        case['_recipe']=dict(family='token_spell_extension',version=2,variant=case['_recipe']['variant'],params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)
    return result
