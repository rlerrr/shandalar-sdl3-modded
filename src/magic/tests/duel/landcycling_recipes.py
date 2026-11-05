"""Oracle-pinned landcycling components using actual hand activation."""
import re
from land_search_recipes import TYPES,variants as search_variants,validate_witnesses as search_witnesses,expand as search_expand
from cycling_recipes import validate_witnesses as cycling_witnesses


def spec(d,r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
        raise ValueError('landcycling requires a whole pinned normal card and explicit variants')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'cycling_cost','basic','types'} or type(p['basic']) is not bool:
        raise ValueError('explicit cycling cost and basic/type restrictions required')
    cost=p['cycling_cost'];types=p['types']
    if not isinstance(cost,str) or not re.fullmatch(r'(?:\{(?:[1-7]|[BUGRW])\})+',cost):raise ValueError('bounded ordinary landcycling cost required')
    if not isinstance(types,list) or any(t not in TYPES for t in types) or len(types)!=len(set(types)) or (types and p['basic']) or (not p['basic'] and not 1<=len(types)<=2):
        raise ValueError('basic landcycling or one/two distinct printed land types required')
    pool=cost_from_oracle(dict(mana_cost=cost))
    if p['basic']:label='Basic landcycling '+cost;noun='basic land'
    else:
        label=types[0]+'cycling '+cost
        if len(types)==2:label+=', '+types[1].lower()+'cycling '+cost
        noun=' or '.join(types)
    article='an' if noun.startswith('Island') else 'a'
    reminder=f' ({cost}, Discard this card: Search your library for {article} {noun} card, reveal it, put it into your hand, then shuffle.)'
    allowed={label,label+reminder};lines=r['oracle_text'].splitlines();matching=[l for l in lines if l in allowed];other=[l for l in lines if l not in allowed]
    if len(matching)!=1 or 'Cycling' not in r.get('keywords',[]) or re.search(r'cycl|madness|unearth|flashback','\n'.join(other),re.I):
        raise ValueError('one reviewed landcycling clause without additional cycling/discard mechanics required')
    # The remaining text is pinned, visible provenance. Main-phase hand
    # activation does not cast this card or exercise its battlefield/upkeep rules.
    if re.search(r'\b(?:when|whenever)\b[^\n]*(?:discard|from your hand|graveyard)','\n'.join(other),re.I):
        raise ValueError('additional discard/zone-change triggers require dedicated cases')
    return dict(family='landcycling',mode='landcycling',basic=p['basic'],types=list(types),snow=False,destination='hand',
                cost=pool,spell_mana=[0]*6,tap=False,sacrifice=False,pay_life=0,enters_tapped=False,mana_ability=False,creature=False,
                untested_rules=other,untested_keywords=[k for k in r.get('keywords',[]) if k not in {'Cycling','Typecycling','Landcycling','Basic landcycling',*(t+'cycling' for t in TYPES)}])


def variants(p):return search_variants(p)


def validate_witnesses(o,rows):
    search_witnesses(o,rows);cycling_witnesses(o,rows)


def expand(name,csvid,d,p,selected,rows,o):return search_expand(name,csvid,d,p,selected,rows,o)
