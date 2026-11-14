"""Whole-Oracle entry land searches with real permanent and dynamic P/T envelopes."""
import copy
import json
import re
from pathlib import Path

from land_search_recipes import eligible as land_eligible
from library_search_recipes import wave6_exact as exact_pin
from entry_trigger_value_selection_recipes import SelectionLedger

FAMILY='entry_land_tutor'
DATA=Path(__file__).parent/'data/entry_land_tutor_wave8_profiles.json'
LAND_PALETTE=['Plains','Swamp','Forest','Snow-Covered Plains','Snow-Covered Swamp','Snow-Covered Forest',
              'Tundra','Underground Sea','Savannah','Taiga','Darksteel Citadel','Scrying Sheets','Dryad Arbor','Elvish Mystic','Tarfire']


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    types=set(r.get('type_line','').partition(' — ')[0].split()) if r else set()
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or not types&{'Creature','Artifact'}:
        raise ValueError('land entry requires actual normal Creature or Artifact')
    prefix=r'When (?:this creature|this artifact|'+re.escape(r['name'])+r') enters'
    clauses=[line for line in r['oracle_text'].splitlines() if re.match(prefix,line)]
    if len(clauses)!=1:raise ValueError('one complete own entry land search clause required')
    clause=clauses[0];body=re.sub(prefix,'',clause,count=1);death_search=body.startswith(' or is put into a graveyard from the battlefield, ')
    body=body.removeprefix(' or is put into a graveyard from the battlefield')
    body=body.removeprefix(', ');conditional=body.startswith('if an opponent controls more lands than you, ')
    body=body.removeprefix('if an opponent controls more lands than you, ')
    count_gate='none';quantity=1
    m=re.fullmatch(r'search your library for up to X Plains cards, where X is the number of players who control more lands than you\. Reveal those cards, put them into your hand, then shuffle\.',body)
    if m:basic=False;types_filter=['Plains'];destination='hand';optional=False;count_gate='players_with_more_lands';quantity='land_comparison'
    else:
        m=re.fullmatch(r'you may search your library for a (basic land|Swamp|Forest|Plains|land) card, (reveal it, put it into your hand|put (?:it|that card) onto the battlefield(?: tapped)?(?: under target player\'s control)?), then shuffle\.',body)
        if not m:raise ValueError('unsupported complete land entry clause')
        basic=m[1]=='basic land';types_filter=[m[1]] if m[1] in ('Swamp','Forest','Plains') else []
        destination='hand' if m[2].startswith('reveal') else 'battlefield_tapped' if ' tapped' in m[2] else 'battlefield'
        optional=True
    lines=r['oracle_text'].splitlines();dynamic=f"{r['name']}'s power and toughness are each equal to the number of lands you control." in lines
    if 'Creature' in types and not dynamic and any(not str(r.get(k,'')).isdigit() for k in ('power','toughness')):raise ValueError('unreviewed source stats')
    death_draw='When this creature dies, you may draw a card.' in lines
    pump='{B}: This creature gets +1/+1 until end of turn.' in lines
    covered=[clause]+(['When this creature dies, you may draw a card.'] if death_draw else [])+([f"{r['name']}'s power and toughness are each equal to the number of lands you control."] if dynamic else [])+(['{B}: This creature gets +1/+1 until end of turn.'] if pump else [])
    return dict(predicate=dict(types=types_filter,basic=basic,snow=False),destination=destination,quantity=quantity,
                optional=optional,target_player='target player' in clause,condition='opponent_more' if conditional else count_gate,
                death_search=death_search,death_draw=death_draw,pump=pump,dynamic_stats=dynamic,entry_clause=clause,
                gaps=[line for line in lines if line not in covered])


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):raise ValueError('explicit version2 land entry variants required')
    q=d.get('params');parsed=mechanism(r);profile=pins()['sources'].get(r['name'])
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=1:raise ValueError('land entry schema required')
    if not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not exact_pin(q['mechanism'],parsed) or not exact_pin(q['presentation'],profile['presentation']):raise ValueError('full land source/predicate/presentation pin changed')
    if any(type(v) is not bool for v in q['presentation'].values()):raise ValueError('typed actual UI annotations required')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],scope='land',presentation=copy.deepcopy(q['presentation']),mana=cost_from_oracle(r),creature='Creature' in r['type_line'].partition(' — ')[0].split())


def variants(p):
    modes=['search','payment','insufficient_mana','countered','accepted_zero','no_match','empty_library','single_match','last_choice','duplicates',
           'cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','bounce_recast','nonmatching_entry','source_destroyed']
    if any(p['mana'][1:]):modes.append('wrong_color')
    if p['creature']:modes.append('blink_source')
    if p['target_player']:modes+=['opponent_recipient','recipient_owner_return']
    if p['condition']!='none':modes+=['equal_lands','own_more_lands']
    if p['dynamic_stats']:modes+=['land_removed','artifact_land_count','land_creature_count']
    if p['death_search'] or p['death_draw']:modes+=['other_destroyed','source_bounced']
    if p['pump']:modes+=['pump','pump_expiry']
    modes += ['land_'+str(i) for i in range(len(LAND_PALETTE))]
    result=[f'{m}_{o}' for o in (0,1) for m in modes]
    if p['optional'] and p['presentation']['human_entry_trigger']:result.append('whole_decline_0')
    return result


def validate_witnesses(oracle,rows):
    for name,record in pins()['witnesses'].items():
        if name not in rows or not exact_pin(record,oracle.get(name)):raise ValueError('land tutor full witness pin changed: '+name)
    for name,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(name)) or rows[name]['csvid']!=p['csvid'] or rows[name]['kind']!='card' or rows[name]['handler'] in (None,'NULL','card_dummy'):raise ValueError('land source pin/implementation changed')


class LandLedger(SelectionLedger):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs);self.shuffles=[0,0];self.inspected=None;self.fetched=[]

    def land_count(self,player):
        return sum(e['zone']=='battlefield' and e['player']==player and 'Land' in self.oracle[e['name']]['type_line'].partition(' — ')[0].split() for e in self.entities.values())

    def receipts(self):
        from recipes import step
        out=[]
        for item in super().receipts():
            s=dict(item)
            if s['op']=='assert_shuffle_count':s['value']=self.shuffles[s['player']]
            if s['op']=='assert_zone_card' and s['zone']=='library' and self.shuffles[s['player']]:continue
            out.append(s)
        if self.inspected:
            out += [step('assert_offered_count',value=len(self.inspected)),step('assert_offered_selection_required',value=0)]
            out += [step('assert_offered_legal_card',index=self.rows[n]['csvid'],value=int(land_eligible(self.oracle[n],self.p['predicate']))) for n in sorted(set(self.inspected))]
        return out


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    def add(n,ref,player,zone='hand'):setup.append(card(n,ref,player,zone))
    add(name,'source',owner)
    for player in (0,1):
        add('Giant Growth',f'hand_{player}',player);add('Mountain',f'grave_{player}',player,'graveyard');add('Swamp',f'exile_{player}',player,'exile')
        count=1 if player==owner else 2
        if mode=='equal_lands':count=1
        if mode=='own_more_lands':count=2 if player==owner else 1
        for i in range(count):
            n='Darksteel Citadel' if mode=='artifact_land_count' and player==owner else 'Dryad Arbor' if mode=='land_creature_count' and player==owner else 'Forest'
            # A single untapped Forest would legally pay a missing sole {G}.
            # Keep the denial fixture's real land count, with an off-color land.
            if mode=='wrong_color' and player==owner and p['mana'][3]==1 and next(c for c in range(1,6) if p['mana'][c])==3:n='Swamp'
            add(n,f'board_land_{player}_{i}',player,'battlefield')
    library=list(LAND_PALETTE)
    if mode=='no_match':library=['Elvish Mystic','Tarfire']
    if mode=='empty_library':library=[]
    if mode=='single_match':library=[next(n for n in LAND_PALETTE if land_eligible(oracle[n],p['predicate'])),'Tarfire']
    if mode=='duplicates':library=[next(n for n in LAND_PALETTE if land_eligible(oracle[n],p['predicate']))]*3+['Tarfire']
    if mode=='source_destroyed' and p['death_draw']:library=['Forest']*4
    if mode.startswith('land_') and mode[5:].isdigit():library=[LAND_PALETTE[int(mode[5:])],'Tarfire']
    # Death-draw fixtures retain a nonempty library after an accepted search.
    for player in (0,1):
        for i,n in enumerate(library):add(n,f'library_{player}_{i}',player,'library')
    if mode in ('bounce_recast','source_bounced','recipient_owner_return'):add('Boomerang','bounce',owner)
    if mode=='blink_source':add('Cloudshift','blink',owner)
    if mode=='countered':add('Counterspell','counter',1-owner)
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner)
    if mode=='other_destroyed':add('Elvish Mystic','other',owner,'battlefield')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=LandLedger(case,p,mode,owner,rows,oracle)
    pools=[[0]*6,[0]*6];actions=[step('observe_search_filters',value=1),step('turn',player=owner,value=0)];pump=0
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(player,vector):
        pools[player]=list(vector);actions.extend(step('set_mana',player=player,color=i,value=n) for i,n in enumerate(vector))
    def paid(player,n):
        pools[player]=[a-b for a,b in zip(pools[player],cost(n))];assert min(pools[player])>=0
    def receipt():
        out=[step('assert_stack',value=0),step('assert_choices_consumed')]+w.receipts()
        out += [step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player])]
        e=w.entities['source'];alive=e['zone']=='battlefield'
        out += [step('assert_card',ref='source',property='in_play',value=int(alive)),step('assert_card',ref='source',property='in_hand',value=int(e['zone']=='hand'))]
        if alive and p['creature']:
            out += [step('assert_card',ref='source',property=k,value=(w.land_count(owner) if p['dynamic_stats'] else int(oracle[name][k]))+pump) for k in ('power','toughness')]
        for ref in w.fetched:
            if w.entities[ref]['zone']=='battlefield':out += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(w.entities[ref]['tapped']))]
        out += [step('assert_life',player=player,value=20) for player in (0,1)]
        return out
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));paid(e['player'],e['name']);w.move(ref,'stack')
        actions.extend([step('assert_card',ref=ref,property='in_hand',value=0),step('assert_stack',value=2 if response else 1)])
        actions.extend(step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player]))
    def search(death=False):
        if mode=='whole_decline':return
        if p['condition']=='opponent_more' and w.land_count(1-owner)<=w.land_count(owner):return
        if owner==0 and p['presentation']['human_death_trigger' if death else 'human_entry_trigger']:actions.append(step('choose_trigger',player=owner,ref='source'))
        cap=int(w.land_count(1-owner)>w.land_count(owner)) if p['quantity']=='land_comparison' else 1
        recipient=1-owner if mode in ('opponent_recipient','recipient_owner_return') else owner
        if p['target_player']:actions.append(step('choose_player',player=owner,value=recipient))
        deck=w.zones['library'][owner];legal=[n for n in deck if land_eligible(oracle[n],p['predicate'])]
        chosen=[] if mode=='accepted_zero' or cap==0 else (legal[-1:] if mode=='last_choice' else legal[:1])
        w.inspected=list(deck) if cap and deck else None
        if cap and deck:
            if chosen:actions.append(step('choose_zone_card',player=owner,value=rows[chosen[0]]['csvid']))
            else:actions.append(step('choose_optional_zone',player=owner,value=rows[(legal or deck)[0]]['csvid']))
        for n in chosen:
            deck.remove(n)
            if p['destination']=='hand':w.zones['hand'][owner].append(n)
            else:
                ref='found_'+str(len(w.fetched));w.entities[ref]=dict(name=n,player=recipient,zone='battlefield',tapped=p['destination']=='battlefield_tapped',owner=owner)
                w.fetched.append(ref)
        w.shuffles[owner]+=1
        return chosen
    def bind_found(before):
        for ref in w.fetched[before:]:
            e=w.entities[ref];same=[r for r in w.fetched if r!=ref and w.entities[r]['name']==e['name'] and w.entities[r]['player']==e['player'] and w.entities[r]['zone']=='battlefield']
            prior=sum(1 for r in w.fetched[:w.fetched.index(ref)] if r in same)
            prior+=sum(1 for r,e2 in w.entities.items() if r not in w.fetched and e2['zone']=='battlefield' and e2['player']==e['player'] and e2['name']==e['name'])
            actions.append(step('bind_permanent_copy',player=e['player'],value=rows[e['name']]['csvid'],index=prior,ref=ref))
    def enter():
        w.move('source','battlefield');before=len(w.fetched);search();actions.append(step('resolve'));bind_found(before);actions.extend(receipt())
    def bounce(ref):
        funds(owner,cost('Boomerang'));actions.append(step('choose_target',player=owner,ref=ref));cast('bounce');actions.append(step('resolve'));w.move('bounce','graveyard')
        e=w.entities[ref]
        if ref in w.fetched:
            e['zone']='hand';w.zones['hand'][e['owner']].append(e['name'])
        else:w.move(ref,'hand')
        actions.append(step('assert_choices_consumed'))
    pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':
        c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
    funds(owner,pool);funds(1-owner,cost('Counterspell') if mode=='countered' else [0]*6);actions.append(step('refresh'));ready=len(actions)
    if mode in ('insufficient_mana','wrong_color'):
        actions.append(step('cast',ref='source',value=0));pending=len(actions);actions.extend(receipt())
    else:
        cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',response=True);actions.append(step('resolve_response'));w.move('source','graveyard');w.move('counter','graveyard');actions.extend(receipt())
        else:enter()
    if mode=='bounce_recast':
        bounce('source');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));funds(owner,p['mana']);cast('source');enter()
    if mode=='source_bounced':
        bounce('source');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipt())
    if mode=='blink_source':
        funds(owner,cost('Cloudshift'));actions.append(step('choose_target',player=owner,ref='source'));cast('blink');w.move('blink','graveyard');before=len(w.fetched);search();actions.append(step('resolve'));actions.append(step('bind_permanent',player=owner,value=csvid,ref='source'));bind_found(before);actions.extend(receipt())
    if mode=='nonmatching_entry':
        funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipt())
    if mode in ('source_destroyed','other_destroyed'):
        ref='other' if mode=='other_destroyed' else 'source';before=len(w.fetched)
        if ref=='source' and p['death_search']:search(death=True)
        if ref=='source' and p['death_draw'] and owner==0 and p['presentation']['human_death_trigger']:actions.append(step('choose_trigger',player=owner,ref='source'))
        actions.append(step('destroy',ref=ref));w.move(ref,'graveyard')
        if ref=='source' and p['death_draw']:
            # Identical independent witnesses make the real shuffled draw exact
            # without controlling or reading its random post-search top card.
            if not w.zones['library'][owner]:raise ValueError('death draw requires nonempty actual library')
            if len(set(w.zones['library'][owner]))!=1:raise ValueError('death draw fixture needs homogeneous shuffled witnesses')
            w.zones['hand'][owner].append(w.zones['library'][owner].pop(0))
        actions.append(step('resolve'));bind_found(before);actions.extend(receipt())
    if mode=='recipient_owner_return':
        if not w.fetched:raise ValueError('recipient ownership probe requires a fetched Forest')
        bounce(w.fetched[-1]);actions.extend(receipt())
    if mode=='land_removed':actions.append(step('destroy',ref=f'board_land_{owner}_0'));w.move(f'board_land_{owner}_0','graveyard');actions.extend(receipt())
    if mode in ('pump','pump_expiry'):
        funds(owner,[0,1,0,0,0,0]);actions.extend([step('require_can_activate',ref='source',value=1),step('activate',ref='source',value=1),step('resolve')]);pools[owner][1]-=1;pump=1;actions.extend(receipt())
        if mode=='pump_expiry':actions.append(step('cleanup'));pump=0;actions.extend(receipt())
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipt())
    if mode in ('ready_snapshot','pending_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    gaps=p['gaps']+['independent ETB priority/trigger-order alternatives','multiplayer','replacement effects','land-word changing effects','optional death-draw decline UI']
    case.update(name=f'EntryLandTutorRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('land tutor identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
