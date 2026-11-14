"""Real entry tap/untap effects, optional choices, Flash and next untap steps."""
import copy
import json
import re
from pathlib import Path

from library_search_recipes import wave6_exact as exact_pin
from tap_spell_recipes import matches as target_matches
from entry_trigger_value_selection_recipes import SelectionLedger

FAMILY='entry_tap_trigger'
DATA=Path(__file__).parent/'data/entry_tap_trigger_wave8_profiles.json'


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    if (not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line','').partition(' — ')[0].split()
            or any(not str(r.get(k,'')).isdigit() for k in ('power','toughness'))):raise ValueError('entry tap requires a whole normal fixed-stat Creature')
    prefix=r'When (?:this creature|'+re.escape(r['name'])+r') enters, '
    clauses=[line for line in r['oracle_text'].splitlines() if re.match(prefix,line)]
    if len(clauses)!=1:raise ValueError('one complete entry tap clause required')
    body=re.sub(prefix,'',clauses[0],count=1)
    m=re.fullmatch(r"(you may )?tap( or untap)? target (permanent|creature|land)( an opponent controls)?\.(?: (?:That creature|That land|It) doesn't untap during its controller's next untap step\.)?",body)
    if not m:raise ValueError('unreviewed complete entry tap grammar')
    extra=[line for line in r['oracle_text'].splitlines() if line!=clauses[0]]
    allowed=lambda line:line in ('Flying','This creature enters tapped.') or re.fullmatch(r'Flash(?: \(.*\))?',line)
    if any(not allowed(line) for line in extra) or set(r.get('keywords',[]))-{'Flying','Flash'}:raise ValueError('new active clause requires a separate entry model')
    return dict(scope=m[3],controller='opponent' if m[4] else 'any',optional=bool(m[1]),effect='twiddle' if m[2] else 'tap',
                next_untap="doesn't untap" in body,flash='Flash' in r.get('keywords',[]),flying='Flying' in r.get('keywords',[]),
                enters_tapped='This creature enters tapped.' in extra,entry_clause=clauses[0],other_clauses=extra)


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):raise ValueError('explicit version2 entry tap variants required')
    q=d.get('params');parsed=mechanism(r);profile=pins()['sources'].get(r['name'])
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=1:raise ValueError('entry tap schema required')
    if (not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text']
            or not exact_pin(q['mechanism'],parsed) or not exact_pin(q['presentation'],profile['presentation'])):raise ValueError('whole entry tap Oracle/profile changed')
    if set(q['presentation'])!={'human_optional_trigger','present_ai_twiddle'} or any(type(v) is not bool for v in q['presentation'].values()):raise ValueError('actual typed UI annotations required')
    if q['presentation']['present_ai_twiddle']!=(parsed['effect']=='twiddle'):raise ValueError('twiddle menu presentation must match actual effect')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],presentation=copy.deepcopy(q['presentation']),mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']))


def variants(p):
    modes=['tap','payment','insufficient_mana','wrong_color','countered','already_tapped','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot',
           'bounce_recast','blink_source','nonmatching_entry','source_bounced','source_destroyed']
    if p['optional']:modes+=['decline','own_target','self_target']
    if p['controller']=='opponent':modes.append('no_targets')
    if p['next_untap']:
        modes+=['next_untap','source_departure_lock','freeze_cleanup']
        if p['scope']=='creature':modes.append('frozen_attack')
    if p['effect']=='twiddle':modes+=['untap','already_untapped','own_untap']
    if p['scope']=='permanent':modes+=['artifact_target','land_target','enchantment_target','artifact_function']
    if p['flash']:modes.append('offturn')
    if p['flying']:modes+=['block_flying','flying_evasion','flying_reach']
    result=[f'{m}_{owner}' for owner in (0,1) for m in modes]
    if p['presentation']['human_optional_trigger']:result.append('whole_decline_0')
    return result


def validate_witnesses(oracle,rows):
    for n,r in pins()['witnesses'].items():
        if n not in rows or not exact_pin(r,oracle.get(n)):raise ValueError('entry tap full witness pin changed: '+n)
    for n,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(n)) or rows[n]['csvid']!=p['csvid'] or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL','card_dummy'):raise ValueError('entry tap source identity/implementation changed')


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    def add(n,ref,player,zone='battlefield',tapped=False):
        item=card(n,ref,player,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']
        setup.append(item)
    add(name,'source',owner,'hand')
    for player in (0,1):
        add('Giant Growth',f'hand_{player}',player,'hand');add('Mountain',f'grave_{player}',player,'graveyard');add('Island',f'exile_{player}',player,'exile')
        for i in range(2):add('Forest',f'library_{player}_{i}',player,'library')
        if not (mode=='no_targets' and p['scope']=='creature' and player!=owner):
            n='Suntail Hawk' if mode=='block_flying' and p['flash'] and player!=owner else 'Giant Spider' if mode=='flying_reach' and player!=owner else 'Elvish Mystic'
            add(n,f'creature_{player}',player)
        add('Sol Ring',f'artifact_{player}',player)
        if not (mode=='no_targets' and p['scope']=='land' and player!=owner):add('Swamp',f'land_{player}',player)
        add('Seal of Cleansing',f'enchantment_{player}',player)
    if mode=='block_flying' and not p['flash']:add('Suntail Hawk','flying_attacker',1-owner)
    target=f'land_{1-owner}' if p['scope']=='land' else f'creature_{1-owner}'
    if mode in ('own_target','own_untap'):target=f'creature_{owner}'
    if mode=='self_target':target='source'
    if mode in ('artifact_target','artifact_function'):target=f'artifact_{owner}'
    if mode=='land_target':target=f'land_{owner}'
    if mode=='enchantment_target':target=f'enchantment_{owner}'
    untap=mode in ('untap','own_untap','already_untapped','artifact_function')
    initial_tapped=mode=='already_tapped' or untap and mode!='already_untapped'
    for item in setup:
        if item['ref']==target and item['zone']=='battlefield':item['state']=['tapped' if initial_tapped else 'untapped']
    if mode in ('bounce_recast','source_bounced','source_departure_lock'):add('Boomerang','bounce',owner,'hand')
    if mode=='blink_source':add('Cloudshift','blink',owner,'hand')
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode=='artifact_function':add('Mind Stone','functional_stone',owner,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=SelectionLedger(case,p,mode,owner,rows,oracle)
    for item in setup:
        if item['zone']=='battlefield':w.entities[item['ref']]['tapped']='tapped' in item.get('state',[])
    actions=[];pools=[[0]*6,[0]*6];life=[20,20];locks={};current=1-owner if mode=='offturn' or mode=='block_flying' and p['flash'] else owner
    actions.append(step('turn',player=current,value=0))
    if p['presentation']['present_ai_twiddle']:actions.append(step('present_ai_dialogs',value=1))
    if mode=='block_flying' and p['flash']:
        actions += [step('begin_attackers'),step('attack',ref=f'creature_{1-owner}',value=1),step('finish_attackers')];w.entities[f'creature_{1-owner}']['tapped']=True
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(player,vector):
        pools[player]=list(vector);actions.extend(step('set_mana',player=player,color=i,value=n) for i,n in enumerate(vector))
    def pay(player,n):
        pools[player]=[a-b for a,b in zip(pools[player],cost(n))];assert min(pools[player])>=0
    def receipt():
        out=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=0)]+w.receipts()
        out += [step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player])]
        out += [step('assert_life',player=player,value=life[player]) for player in (0,1)]
        for ref,e in w.entities.items():
            if e['zone']=='battlefield':out += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
        e=w.entities['source'];out += [step('assert_card',ref='source',property='in_hand',value=int(e['zone']=='hand')),step('assert_card',ref='source',property='in_play',value=int(e['zone']=='battlefield'))]
        if e['zone']=='battlefield':out += [step('assert_card',ref='source',property=k,value=p[k]) for k in ('power','toughness')]
        return out
    def combat_receipt(marked):
        out=receipt()
        out += [step('assert_card',ref=ref,property='damage',value=marked.get(ref,0)) for ref,e in w.entities.items()
                if e['zone']=='battlefield' and 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split()]
        return out
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],e['name']);w.move(ref,'stack')
        actions.extend([step('assert_card',ref=ref,property='in_hand',value=0),step('assert_stack',value=2 if response else 1)])
        actions.extend(step('assert_mana',player=player,color=i,value=n) for player in (0,1) for i,n in enumerate(pools[player]))
    def effect():
        if mode=='whole_decline':return
        if owner==0 and p['presentation']['human_optional_trigger']:actions.append(step('choose_trigger',player=owner,ref='source'))
        if mode=='no_targets':
            probe=f'land_{owner}' if p['scope']=='land' else f'creature_{owner}'
            actions.append(step('choose_optional_target',player=owner,ref=probe));return
        e=w.entities[target]
        if not target_matches(oracle[e['name']],p,owner,e['player']):raise ValueError('selected entry tap witness is not Oracle legal')
        if mode=='decline':actions.append(step('choose_optional_target',player=owner,ref=target));return
        actions.append(step('choose_target',player=owner,ref=target))
        if p['effect']=='twiddle':actions.append(step('choose_dialog',player=owner,value=int(untap)))
        e['tapped']=not untap
        if p['next_untap']:locks[target]=1
    def enter():
        w.move('source','battlefield');w.entities['source']['tapped']=p['enters_tapped'];effect();actions.append(step('resolve'))
        if mode=='no_targets':actions.append(step('assert_unused_choices',value=1))
        actions.extend(receipt())
    def bounce_source():
        funds(owner,cost('Boomerang'));actions.append(step('choose_target',player=owner,ref='source'));cast('bounce');actions.append(step('resolve'));w.move('bounce','graveyard');w.move('source','hand')
        actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipt())
    def advance_untap():
        nonlocal current
        actions.append(step('advance_turn'));current=1-current;pools[0]=[0]*6;pools[1]=[0]*6
        actions.append(step('untap_step',player=current))
        for ref,e in w.entities.items():
            if e['zone']!='battlefield' or e['player']!=current:continue
            if locks.get(ref,0):locks[ref]-=1
            else:e['tapped']=False
        actions.extend(receipt())
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
    if mode in ('source_bounced','source_departure_lock'):bounce_source()
    if mode=='bounce_recast':bounce_source();funds(owner,p['mana']);cast('source');enter()
    if mode=='blink_source':
        funds(owner,cost('Cloudshift'));actions.append(step('choose_target',player=owner,ref='source'));cast('blink');w.move('blink','graveyard');w.entities['source']['tapped']=p['enters_tapped'];effect();actions.append(step('resolve'));actions.append(step('bind_permanent',player=owner,value=csvid,ref='source'));actions.extend(receipt())
    if mode=='source_destroyed':actions.append(step('destroy',ref='source'));w.move('source','graveyard');actions.extend(receipt())
    if mode=='nonmatching_entry':funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipt())
    if mode in ('cleanup','freeze_cleanup'):actions.append(step('cleanup'));actions.extend(receipt())
    if mode in ('next_untap','source_departure_lock','freeze_cleanup','frozen_attack'):
        advance_untap()
        if mode=='frozen_attack':
            actions += [step('turn',player=current,value=0),step('begin_attackers'),step('attack',ref=target,value=0),step('finish_attackers'),step('assert_card',ref=target,property='attacking',value=0),
                        step('begin_blockers'),step('advance_combat'),step('combat_damage')]
            actions.extend(combat_receipt({}));actions.append(step('end_combat'));actions.extend(combat_receipt({}))
        advance_untap();advance_untap()
    if mode=='artifact_function':
        actions += [step('require_can_activate',ref=target,value=1),step('activate',ref=target,value=1)];w.entities[target]['tapped']=True;pools[owner][0]+=2;actions.extend(receipt())
        cast('functional_stone');actions.append(step('resolve'));w.move('functional_stone','battlefield');w.entities['functional_stone']['tapped']=False;actions.extend(receipt())
    if mode=='block_flying':
        attacker=f'creature_{1-owner}'
        if not p['flash']:
            # The source was really cast on its own turn. A different Flying
            # attacker is not the ETB's frozen target on the later enemy turn.
            advance_untap();attacker='flying_attacker'
            actions += [step('turn',player=current,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),step('finish_attackers')];w.entities[attacker]['tapped']=True
        actions += [step('begin_blockers'),step('block',ref='source',other=attacker,value=1),step('assert_card',ref='source',property='blocking_count',value=1)];actions.extend(receipt())
    if mode in ('flying_evasion','flying_reach'):
        advance_untap();advance_untap()
        if p['next_untap']:
            # A frozen blocker cannot defend on the first later own turn;
            # its second real untap must occur before this attack control.
            advance_untap();advance_untap()
        allowed=int(mode=='flying_reach');actions += [step('turn',player=current,value=0),step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref=f'creature_{1-owner}',other='source',value=allowed)]
        if not allowed:actions.append(step('assert_card',ref='source',property='blocked',value=0))
        w.entities['source']['tapped']=True;actions.extend(receipt())
        actions.append(step('advance_combat'))
        if allowed:actions.append(step('assert_card',ref='source',property='blocked',value=1))
        actions.append(step('combat_damage'))
        marked={}
        if allowed:
            # Both combatants deal their actual printed power simultaneously.
            # A larger source can survive instead of following Pestermite's fate.
            defender=f'creature_{1-owner}';r=oracle[w.entities[defender]['name']]
            if int(r['power'])>=p['toughness']:w.move('source','graveyard')
            else:marked['source']=int(r['power'])
            if p['power']>=int(r['toughness']):w.move(defender,'graveyard')
            else:marked[defender]=p['power']
        else:life[1-owner]-=p['power']
        actions.extend(combat_receipt(marked));actions.append(step('end_combat'));actions.extend(combat_receipt(marked))
    if mode in ('ready_snapshot','pending_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    case.update(name=f'EntryTapTriggerRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
        covered_clauses=[p['entry_clause']]+p['other_clauses'],untested_components=['independent ETB priority/trigger-order alternatives','control changes/phasing','full upkeep/draw turn progression','replacement effects','simultaneous same-name entry triggers','combat damage beyond block legality']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('entry tap identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
