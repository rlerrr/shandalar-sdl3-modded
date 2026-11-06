"""Single-land library searches, using real spells and activated abilities."""
import copy
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

TYPES = ('Plains','Island','Swamp','Mountain','Forest')
DUALS = {'Tundra':('Plains Island','{W} or {U}'),
         'Underground Sea':('Island Swamp','{U} or {B}'),
         'Badlands':('Swamp Mountain','{B} or {R}'),
         'Taiga':('Mountain Forest','{R} or {G}'),
         'Savannah':('Forest Plains','{G} or {W}')}
LANDS = list(TYPES)+['Snow-Covered '+t for t in TYPES]+list(DUALS)+['Darksteel Citadel','Scrying Sheets']
DECK = ['Grizzly Bears','Giant Growth']+LANDS+['Forest','Lightning Bolt']
SPELLS = {'Twiddle':('{U}','You may tap or untap target artifact, creature, or land.'),
          'Counterspell':('{U}{U}','Counter target spell.'),
          'Suppression Field':('{1}{W}',"Activated abilities cost {2} more to activate unless they're mana abilities.")}


def eligible(record, p):
    typ=record['type_line'];base,_,sub=typ.partition(' — ')
    return ('Land' in base.split() and (not p['basic'] or 'Basic' in base.split())
            and (not p['snow'] or 'Snow' in base.split())
            and (not p['types'] or bool(set(p['types']) & set(sub.split()))))


def spec(d,r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or d.get('oracle_text')!=r.get('oracle_text') or 'variants' not in d:
        raise ValueError('single-land search requires a pinned normal card and explicit variants')
    p=d.get('params')
    if not isinstance(p,dict) or set(p)!={'mode','types','basic','snow','destination','activation_cost','tap','sacrifice','pay_life','enters_tapped','mana_ability'}:
        raise ValueError('explicit complete search predicate, destination and cost required')
    if p['mode'] not in ('spell','activated') or p['destination'] not in ('hand','battlefield','battlefield_tapped'):
        raise ValueError('reviewed single-land spell/activation destination required')
    if any(type(p[k]) is not bool for k in ('basic','snow','tap','sacrifice','enters_tapped','mana_ability')) or type(p['pay_life']) is not int or not 0<=p['pay_life']<=3:
        raise ValueError('boolean restrictions and bounded life cost required')
    if not isinstance(p['types'],list) or any(t not in TYPES for t in p['types']) or len(p['types'])!=len(set(p['types'])) or p['snow'] and (p['basic'] or p['types']):
        raise ValueError('reviewed unique basic land types or snow restriction required')
    if not isinstance(p['activation_cost'],str) or p['activation_cost'] and not re.fullmatch(r'(?:\{(?:[1-9]|[BUGRW])\})+',p['activation_cost']):
        raise ValueError('ordinary bounded activation mana required')
    noun=' or '.join(p['types']) if len(p['types'])==2 else ', '.join(p['types'][:-1])+', or '+p['types'][-1] if len(p['types'])>2 else p['types'][0] if p['types'] else 'land'
    noun=('basic ' if p['basic'] else 'snow ' if p['snow'] else '')+noun
    article='an' if noun.startswith('Island') else 'a'
    end=('reveal {ref}, put {ref} into your hand' if p['destination']=='hand' else 'put {ref} onto the battlefield'+(' tapped' if p['destination']=='battlefield_tapped' else ''))
    clauses={f'Search your library for {article} {noun} card, '+end.format(ref=ref)+', then shuffle.' for ref in ('it','that card')}
    lines=r['oracle_text'].splitlines();kind=r['type_line'].split(' — ')[0].split();creature='Creature' in kind
    if creature and any(not re.fullmatch(r'\d+',r.get(k,'')) or not 1<=int(r[k])<=10 for k in ('power','toughness')):
        raise ValueError('numeric surviving search creature required')
    if r.get('keywords'):raise ValueError('search source keywords require separate reviewed scenarios')
    if p['mode']=='spell':
        if not set(kind)&{'Instant','Sorcery'} or any(p[k] for k in ('tap','sacrifice','pay_life','enters_tapped','mana_ability','activation_cost')) or lines[0] not in clauses or len(lines)!=1:
            raise ValueError('ordinary single-clause land search spell required')
    else:
        permanent=next((t for t in ('Creature','Artifact','Enchantment','Land') if t in kind),None)
        if not permanent:raise ValueError('activated search requires a permanent')
        costs=([p['activation_cost']] if p['activation_cost'] else [])+(['{T}'] if p['tap'] else [])+([f"Pay {p['pay_life']} life"] if p['pay_life'] else [])+(['Sacrifice this '+permanent.lower()] if p['sacrifice'] else [])
        if not costs:raise ValueError('explicit nonempty search activation cost required')
        prefix=', '.join(costs)+': '
        expected=[prefix+line for line in clauses]
        if p['enters_tapped']:
            if lines[0]!='This '+permanent.lower()+' enters tapped.':raise ValueError('printed entry tapping required')
            lines=lines[1:]
        if p['mana_ability']:
            if not lines or lines[0]!='{T}: Add {C}.':raise ValueError('reviewed separate colorless ability required')
            lines=lines[1:]
        if len(lines)!=1 or lines[0] not in expected:raise ValueError('search parameters must match the complete Oracle text')
    dialogs=d.get('activation_dialogs',{})
    if not isinstance(dialogs,dict) or any(k not in ('0','1') or not isinstance(v,list) or len(v)>4 or any(type(n) is not int or not 0<=n<=32 for n in v) for k,v in dialogs.items()) or dialogs and not p['mana_ability']:
        raise ValueError('dialog annotations require reviewed separate mana/search menus')
    return dict(p,family='land_search',creature=creature,spell_mana=cost_from_oracle(r) if r.get('mana_cost') else [0]*6,cost=cost_from_oracle(dict(mana_cost=p['activation_cost'])) if p['activation_cost'] else [0]*6)


def variants(p,palette=None):
    palette=LANDS if palette is None else palette
    modes=['search','payment','decline','no_match','empty_library','single_match','duplicates','cleanup',
           'bare_zones','ready_snapshot','pending_snapshot','resolved_snapshot','two_sources']
    modes += [('card_' if p['family']=='library_search' else 'land_')+str(i) for i in range(len(palette))]
    if p['mode']=='landcycling':
        modes+=['opposing_turn','combat','cancel','insufficient_mana','reducer','opposing_reducer','two_reducers',
                'tax','opposing_tax','two_taxes','reducer_tax','reclaim_pending','crypt_pending','cleanup_pending']
        if any(p['cost'][1:]):modes+=['reducer_missing_color']
    elif p['mode'] in ('spell','entry'):modes+=['insufficient_mana','countered']
    else:
        modes+=['opposing_turn','tax']
        if p['tap']:modes+=['tapped','untap']
        if p['enters_tapped']:modes+=['entry_tapped']
        if p['tap'] and p['creature']:modes+=['sick']
        if p['pay_life']:modes+=['insufficient_life']
        if not p['mana_ability'] and sum(p['cost']):modes+=['insufficient_mana']
        if not p['sacrifice']:modes+=['repeat']
    if p['family']=='library_search':
        # A free sibling ability keeps the card-level activation query true.
        # Per-option menu affordability needs a separate observer/scenario.
        if p.get('other_free_ability') and 'insufficient_mana' in modes:modes.remove('insufficient_mana')
        if not p['failure_allowed']:
            modes.remove('no_match')
            if not p['optional']:modes.remove('decline')
        if p['legendary'] and 'two_sources' in modes:modes.remove('two_sources')
        if p['mode']=='entry':modes+=['destroy_source','bounce_recast','blink']
    cost=p['spell_mana'] if p['mode'] in ('spell','entry') else p['cost']
    if any(cost[1:]) and not p['mana_ability']:modes+=['wrong_color']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
    for t,c in zip(TYPES,('W','U','B','R','G')):
        for name,base in ((t,'Basic Land'),('Snow-Covered '+t,'Basic Snow Land')):
            if name not in rows or (o[name]['type_line'],o[name]['oracle_text'])!=(base+' — '+t,'({T}: Add {'+c+'}.)'):raise ValueError('basic/snow witness changed: '+name)
    for name,(typ,mana) in DUALS.items():
        if name not in rows or (o[name]['type_line'],o[name]['oracle_text'])!=('Land — '+typ,'({T}: Add '+mana+'.)'):raise ValueError('dual witness changed: '+name)
    if 'Darksteel Citadel' not in rows or (o['Darksteel Citadel']['type_line'],o['Darksteel Citadel']['oracle_text'])!=('Artifact Land','Indestructible\n{T}: Add {C}.'):raise ValueError('untyped land witness changed')
    if 'Scrying Sheets' not in rows or (o['Scrying Sheets']['type_line'],o['Scrying Sheets']['oracle_text'])!=('Snow Land','{T}: Add {C}.\n{1}{S}, {T}: Look at the top card of your library. If that card is snow, you may reveal it and put it into your hand. ({S} can be paid with one mana from a snow source.)'):raise ValueError('nonbasic snow witness changed')
    for name,fields in SPELLS.items():
        if name not in rows or tuple(o.get(name,{}).get(k) for k in ('mana_cost','oracle_text'))!=fields:raise ValueError('search response witness changed: '+name)
    if tuple(o['Grizzly Bears'].get(k) for k in ('mana_cost','oracle_text','power','toughness'))!=('{1}{G}','','2','2'):raise ValueError('unaffected witness changed')


def expand(name,csvid,d,p,selected,rows,o,palette=None,base_deck=None,match=None,observed_match=None,base_zones=None,attempt_rejected=False):
    from recipes import card,step,observe,cost_from_oracle
    palette=LANDS if palette is None else palette
    base_deck=DECK if base_deck is None else base_deck
    match=eligible if match is None else match
    observed_match=(lambda r,p,target:match(r,p)) if observed_match is None else observed_match
    initial_zones=dict(hand=HAND,graveyard=GRAVE,exile=EXILE) if base_zones is None else base_zones
    result=[]
    for variant in selected:
        v,owner=variant.rsplit('_',1);owner=int(owner);bare=v=='bare_zones';spell_mode=p['mode']=='spell';entry_mode=p['mode']=='entry';hand_mode=p['mode']=='landcycling';casting=spell_mode or entry_mode;hand_source=casting or hand_mode
        decks=[list(base_deck),list(base_deck)];valid=[n for n in palette if match(o[n],p)];target=valid[0] if valid else None;candidate=None
        if v.startswith(('land_','card_')):
            candidate=palette[int(v[5:])];target=candidate if match(o[candidate],p) else None
            decks[owner]=['Giant Growth',candidate,'Lightning Bolt']
        if v=='no_match':decks[owner]=['Grizzly Bears','Giant Growth','Lightning Bolt'];target=None
        if v=='empty_library':decks[owner]=[];target=None
        if v=='single_match':decks[owner]=[target]
        if v=='duplicates':decks[owner]=['Giant Growth',target,target,'Lightning Bolt']
        if v=='decline':target=None
        if p['family']=='library_search' and v=='no_match':decks[owner]=[n for n in base_deck if not match(o[n],p)];target=None
        if v in ('two_sources','repeat'):decks[owner].append(target)
        if v in ('bounce_recast','blink') and p['destination']!='top':decks[owner].append(target)
        zones={z:[[] if bare else list(ns),[] if bare else list(ns)] for z,ns in initial_zones.items()}
        zones['library']=decks;setup=[];entities={};actions=[];mana=[[20]*6,[20]*6];life=[20,20]
        pre=[step('turn',player=1-owner if v=='opposing_turn' else owner,value=0),step('observe_search_filters',value=1)]

        def add(nm,ref,pl,zone='battlefield',state=None):
            item=card(nm,ref,pl,zone)
            if state:item['state']=state
            setup.append(item);entities[ref]=dict(name=nm,player=pl,alive=zone=='battlefield',hand=zone=='hand',tapped=bool(state and 'tapped' in state))
            if zone!='battlefield':zones[zone][pl].append(nm)

        add(name,'source',owner,'hand' if hand_source else 'battlefield',['tapped'] if v in ('tapped','untap') else ['sick'] if v=='sick' else None if v=='entry_tapped' or hand_source else ['untapped'])
        entities['source']['tapped'] |= p['enters_tapped'] and v=='entry_tapped'
        if v=='two_sources':
            add(name,'second',owner,'hand' if hand_source else 'battlefield',None if hand_source else ['untapped'])
        add('Grizzly Bears','friendly',owner);add('Grizzly Bears','enemy',1-owner)
        if v=='tax':add('Suppression Field','taxer',1-owner)
        if hand_mode:
            for i,pl in enumerate([owner] if v in ('reducer','reducer_tax','reducer_missing_color') else [1-owner] if v=='opposing_reducer' else [owner,owner] if v=='two_reducers' else []):add('Fluctuator','reducer_'+str(i),pl)
            if v=='opposing_tax':add('Suppression Field','taxer',owner)
            if v=='two_taxes':add('Suppression Field','taxer',owner);add('Suppression Field','other_taxer',1-owner)
            if v=='reducer_tax':add('Suppression Field','taxer',1-owner)
            if v=='crypt_pending':add("Tormod's Crypt",'crypt',0)
            if v=='reclaim_pending':add('Reclaim','reclaim',owner,'hand')
        for pl in (0,1):
            for z,ns in zones.items():
                if z!='library':initial=[] if bare else initial_zones[z]
                else:initial=list(ns[pl])
                setup[:0]=[card(nm,f'p{pl}_{z}_{i}',pl,z) for i,nm in enumerate(initial)]
        cost=list(p['spell_mana'] if casting else p['cost'])
        if hand_mode:
            taxes=sum(e['name']=='Suppression Field' for e in entities.values());reducers=sum(e['name']=='Fluctuator' and e['player']==owner for e in entities.values())
            cost[0]=max(0,cost[0]+2*taxes-2*reducers)
        elif v=='tax':cost[0]+=2
        if v=='payment' or hand_mode and v in ('reducer','opposing_reducer','two_reducers','tax','opposing_tax','two_taxes','reducer_tax'):mana[owner]=list(cost)
        if v=='insufficient_mana':mana[owner]=[0]*6
        if v in ('wrong_color','reducer_missing_color'):
            mana[owner]=list(cost);c=next(c for c in range(1,6) if cost[c]);mana[owner][1 if c!=1 else 2]+=mana[owner][c];mana[owner][c]=0
        if v=='insufficient_life':life[owner]=p['pay_life']-1;pre.append(step('set_life',player=owner,value=life[owner]))
        for pl in (0,1):pre += [step('set_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
        pre.append(step('refresh'))
        if v=='combat':
            pre += [step('begin_attackers'),step('attack',ref='friendly',value=1),step('finish_attackers')];entities['friendly']['tapped']=True
        denied=v in ('insufficient_mana','wrong_color','reducer_missing_color','tapped','sick','insufficient_life') or v=='entry_tapped' and p['tap']
        fetched=[];top_target=[None]

        def trigger(ref):
            return [step('choose_trigger',player=owner,ref=ref)] if entry_mode and d.get('trigger_priority',False) and owner in d.get('trigger_players',[0]) and not (v=='decline' and p['optional']) else []

        def entry_dialogs():
            if v=='decline' and p.get('optional') and d.get('trigger_priority') and owner in d.get('trigger_players',[0]):return []
            choices=d.get('decline_entry_dialogs' if v=='decline' else 'entry_dialogs',{}).get(str(owner),[])
            if d.get('gate_menu'):choices=[0 if target is not None and 'Gate' in o[target]['type_line'].split(' — ')[-1].split() else 1]
            return [step('choose_dialog',player=owner,value=n) for n in choices]

        def fetched_index():
            return sum(e['alive'] and e['player']==owner and e['name']==target for e in entities.values())+fetched.count(target)-1

        def untap(ref,label):
            add('Twiddle',label,owner,'hand');zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');entities[label]['hand']=False
            mana[owner]=[a-b for a,b in zip(mana[owner],cost_from_oracle(o['Twiddle']))];entities[ref]['tapped']=False
            return [step('choose_target',player=owner,ref=ref),step('cast',ref=label,value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('assert_choices_consumed'),step('refresh')]

        def search(ref='source'):
            e=entities[ref];seq=[]
            inspected=list(decks[owner])
            if not casting and e['tapped'] and p['tap']:seq+=untap(ref,'untap_'+ref)
            if hand_mode:
                seq += [step('require_can_activate_hand',ref=ref,value=1),step('choose_dialog',player=owner,value=1),step('activate_hand',ref=ref,value=1),step('assert_choices_consumed')]
                e['hand']=False;zones['hand'][owner].remove(name)
            elif not casting:
                seq.append(step('require_can_activate',ref=ref,value=1))
                seq += [step('choose_dialog',player=owner,value=n) for n in d.get('activation_dialogs',{}).get(str(owner),[])]
                seq.append(step('activate',ref=ref,value=1));life[owner]-=p['pay_life'];e['tapped']=p['tap']
            else:
                if entry_mode:seq+=trigger(ref)
                seq.append(step('cast',ref=ref,value=1));e['hand']=False;zones['hand'][owner].remove(name)
                if entry_mode:e['alive']=True
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)]
            if spell_mode or hand_mode or p['sacrifice']:
                e['alive']=False;zones['graveyard'][owner].append(name)
            seq.append(step('assert_stack',value=1))
            if p['sacrifice'] or hand_mode:seq += [observe(ref,'in_play',0),step('assert_zone_multiplicity',player=owner,zone='graveyard',index=csvid,value=zones['graveyard'][owner].count(name))]
            # Search resolution is separate from payment and sacrifice.
            seq.append(step('assert_zone',player=owner,zone='library',value=len(decks[owner])))
            if hand_mode:
                seq += [step('assert_mana',player=owner,color=c,value=n) for c,n in enumerate(mana[owner])]
                if v=='cleanup_pending':seq += [step('cleanup'),step('assert_stack',value=1)]
                if v=='reclaim_pending':
                    zones['hand'][owner].remove('Reclaim');zones['graveyard'][owner].remove(name);zones['graveyard'][owner].append('Reclaim');decks[owner].insert(0,name);entities['reclaim']['hand']=False
                    mana[owner]=[a-b for a,b in zip(mana[owner],cost_from_oracle(o['Reclaim']))]
                    seq += [step('choose_zone_card',player=owner,value=csvid),step('cast',ref='reclaim',value=1),step('assert_stack',value=2)]
                if v=='crypt_pending':
                    zones['graveyard'][0].append("Tormod's Crypt");zones['exile'][owner]+=zones['graveyard'][owner];zones['graveyard'][owner]=[];entities['crypt']['alive']=False
                    seq += [step('choose_player',player=0,value=owner),step('activate',ref='crypt',value=1),step('assert_stack',value=2)]
                inspected=list(decks[owner])
            if entry_mode:seq+=entry_dialogs()
            if target:
                seq.append(step('choose_zone_card',player=owner,value=rows[target]['csvid']));decks[owner].remove(target)
                if p['destination']=='hand':zones['hand'][owner].append(target)
                elif p['destination']=='top':decks[owner].insert(0,target);top_target[0]=target
                else:fetched.append(target)
            else:
                seq.append(step('choose_zone_card' if attempt_rejected and candidate is not None else 'choose_optional_zone',player=owner,value=rows[candidate if attempt_rejected and candidate is not None else valid[0]]['csvid']))
            seq += [step('resolve'),step('assert_choices_consumed'),step('assert_forced_zone_choices',value=0),step('refresh')]
            if p['family']=='library_search':life[owner]-=p['lose_life']
            if target:
                seq.append(step('assert_offered_count',value=len(inspected)))
                if p['family']=='library_search':seq.append(step('assert_offered_selection_required',value=int(not p['failure_allowed'])))
                for nm in sorted(set(inspected)):
                    seq.append(step('assert_offered_legal_card',index=rows[nm]['csvid'],value=int(observed_match(o[nm],p,target))))
            if target and p['destination'] in ('battlefield','battlefield_tapped'):
                seq += [step('bind_permanent_copy',player=owner,value=rows[target]['csvid'],index=fetched_index(),ref='found_'+str(len(fetched))),observe('found_'+str(len(fetched)),'tapped',int(p['destination']=='battlefield_tapped'))]
            return seq

        if denied:
            op='activate_hand' if hand_mode else 'cast' if casting else 'activate'
            if not casting:actions += [step('require_can_activate_hand' if hand_mode else 'require_can_activate',ref='source',value=0)]
            actions += [step(op,ref='source',value=0)]
        elif v=='cancel':
            actions += [step('require_can_activate_hand',ref='source',value=1),step('choose_dialog',player=owner,value=0),step('activate_hand',ref='source',value=0),step('assert_choices_consumed')]
        elif v=='countered':
            # The human priority script may counter its own spell legally.
            # This matches the existing response fixtures for either source owner.
            add('Counterspell','counter',0,'hand');zones['hand'][0].remove('Counterspell');zones['graveyard'][0].append('Counterspell');entities['counter']['hand']=False
            zones['hand'][owner].remove(name);zones['graveyard'][owner].append(name);entities['source']['hand']=False
            mana[owner]=[a-b for a,b in zip(mana[owner],cost)];mana[0]=[a-b for a,b in zip(mana[0],cost_from_oracle(o['Counterspell']))]
            actions += [step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
        else:
            actions+=search()
            if v=='two_sources':actions+=search('second')
            if v=='repeat':
                if p['tap']:actions+=untap('source','repeat_untap')
                actions+=search()
            if v=='destroy_source':
                actions.append(step('destroy',ref='source'));entities['source']['alive']=False;zones['graveyard'][owner].append(name)
            if v in ('bounce_recast','blink'):
                nm='Unsummon' if v=='bounce_recast' else 'Cloudshift';ref='reentry_spell';add(nm,ref,owner,'hand');zones['hand'][owner].remove(nm);zones['graveyard'][owner].append(nm);entities[ref]['hand']=False
                mana[owner]=[a-b for a,b in zip(mana[owner],cost_from_oracle(o[nm]))]
                if v=='bounce_recast':
                    # Named tutors can already have a fetched source copy in hand.
                    # Either identical copy can be cast; bind one explicitly.
                    binding=step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source') if name in zones['hand'][owner] else step('bind_hand',player=owner,value=csvid,ref='source')
                    actions += [step('choose_target',player=owner,ref='source'),step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed'),binding]
                    entities['source']['alive']=False;entities['source']['hand']=True;zones['hand'][owner].append(name)
                    actions+=search()
                else:
                    actions += [step('choose_target',player=owner,ref='source'),step('cast',ref=ref,value=1)]
                    actions+=trigger('source')+entry_dialogs()
                    inspected=list(decks[owner])
                    if target:
                        actions.append(step('choose_zone_card',player=owner,value=rows[target]['csvid']));decks[owner].remove(target)
                        if p['destination']=='hand':zones['hand'][owner].append(target)
                        elif p['destination']=='top':decks[owner].insert(0,target);top_target[0]=target
                        else:fetched.append(target)
                    else:actions.append(step('choose_optional_zone',player=owner,value=rows[valid[0]]['csvid']))
                    actions += [step('resolve'),step('assert_choices_consumed'),step('refresh'),step('bind_permanent',player=owner,value=csvid,ref='source')]
                    if target:
                        actions.append(step('assert_offered_count',value=len(inspected)))
                        actions.append(step('assert_offered_selection_required',value=int(not p['failure_allowed'])))
                        actions += [step('assert_offered_legal_card',index=rows[nm]['csvid'],value=int(observed_match(o[nm],p,target))) for nm in sorted(set(inspected))]
                        if p['destination'] in ('battlefield','battlefield_tapped'):
                            actions += [step('bind_permanent_copy',player=owner,value=rows[target]['csvid'],index=fetched_index(),ref='found_'+str(len(fetched))),observe('found_'+str(len(fetched)),'tapped',int(p['destination']=='battlefield_tapped'))]
        pending=next((i+1 for i,s in enumerate(actions) if s['op']=='assert_zone' and s.get('zone')=='library'),0)
        if v=='cleanup':actions += [step('cleanup'),step('refresh')]
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            # A resolved search can reuse a sacrificed/cast source's old slot
            # for the fetched permanent. Exact zones prove those sources left;
            # only inspect references whose cards remain in their original zone.
            if e['alive']:checks += [observe(ref,'in_play',1),observe(ref,'in_hand',0)]
            elif e['hand']:checks += [observe(ref,'in_hand',1)]
            if e['alive']:
                checks.append(observe(ref,'tapped',int(e['tapped'])))
                r=o[e['name']]
                if 'Creature' in r['type_line']:checks += [observe(ref,'power',int(r['power'])),observe(ref,'toughness',int(r['toughness'])),observe(ref,'damage',0)]
        universe=set(initial_zones['hand'])|set(initial_zones['graveyard'])|set(initial_zones['exile'])|set(base_deck+palette+[name]+[e['name'] for e in entities.values()])
        if top_target[0]:checks.append(step('assert_zone_card',player=owner,zone='library',index=0,value=rows[top_target[0]]['csvid']))
        for pl in (0,1):
            checks += [step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values())+(len(fetched) if pl==owner else 0))]
            if v not in ('cleanup','cleanup_pending'):checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
            for z,players in zones.items():
                ns=players[pl];counts=Counter(ns);checks += [step('assert_zone',player=pl,zone=z,value=len(ns))]+[step('assert_zone_multiplicity',player=pl,zone=z,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
                if z=='library' and (pl!=owner or denied or v in ('countered','cancel')):checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=rows[nm]['csvid']) for i,nm in enumerate(ns)]
        seq=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot'):
            cut=0 if v=='ready_snapshot' else pending;tail=actions[cut:]+checks
            seq=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if v=='resolved_snapshot':seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'{"LibrarySearchRecipe" if p["family"]=="library_search" else "LandcyclingRecipe" if hand_mode else "LandSearchRecipe"}_{csvid}_{variant}',card=name,fixture='empty_main',setup=setup,steps=seq,seed=123,
                           _recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=['shuffle randomness distribution','replacement effects','additional trigger ordering','land play timing','separate mana ability'])))
        if hand_mode:
            result[-1]['_recipe'].update(untested_rules=p['untested_rules'],untested_keywords=p['untested_keywords'],untested_components=['casting and spell effects','battlefield abilities','other graveyard abilities','multiplayer','shuffle randomness distribution','replacement effects','additional trigger ordering'])
        if p['family']=='library_search':
            result[-1]['_recipe'].update(untested_rules=p['untested_rules'],untested_keywords=p['keywords'],untested_components=['source keyword combat','other activated abilities','shuffle distribution','additional trigger ordering','replacement effects','control changes','multiplayer'])
    return result
