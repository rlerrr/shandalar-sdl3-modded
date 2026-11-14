"""Whole-Oracle Aura entry tap and persistent attached no-untap effects."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_trigger_value_selection_recipes import SelectionLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY='entry_aura_freeze'
DATA=Path(__file__).parent/'data/entry_aura_freeze_wave8_profiles.json'
PREDICATES={
    'creature':dict(types=['Creature'],power_max=None,colors=[],without_flying=False),
    'creature with power 3 or less':dict(types=['Creature'],power_max=3,colors=[],without_flying=False),
    'red or green creature':dict(types=['Creature'],power_max=None,colors=['R','G'],without_flying=False),
    'creature without flying':dict(types=['Creature'],power_max=None,colors=[],without_flying=True),
    'artifact or creature':dict(types=['Artifact','Creature'],power_max=None,colors=[],without_flying=False),
}
PALETTE=['Elvish Mystic','Goblin Piker','Merfolk of the Pearl Trident','Hill Giant','Craw Wurm','Ornithopter',
         'Giant Spider','Darksteel Relic','Darksteel Citadel','Forest','Seal of Cleansing','Dryad Arbor']


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line')!='Enchantment — Aura':
        raise ValueError('freeze requires one whole normal actual Aura')
    lines=r['oracle_text'].splitlines();flash=False
    if lines and re.fullmatch(r'Flash(?: \(.*\))?',lines[0]):flash=True;lines=lines[1:]
    if len(lines)!=3 or not lines[0].startswith('Enchant '):raise ValueError('complete three-clause attached freeze required')
    phrase=lines[0][8:];q=PREDICATES.get(phrase);noun='permanent' if phrase=='artifact or creature' else 'creature'
    if not q or lines[1]!=f'When this Aura enters, tap enchanted {noun}.' or lines[2]!=f"Enchanted {noun} doesn't untap during its controller's untap step.":
        raise ValueError('unreviewed enchant predicate, entry or persistent duration')
    if set(r.get('keywords',[]))-{'Flash','Enchant'} or ('Flash' in r.get('keywords',[]))!=flash:
        raise ValueError('unreviewed Aura keyword or Flash pin')
    return dict(predicate=copy.deepcopy(q),flash=flash,clauses=r['oracle_text'].splitlines())


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):
        raise ValueError('explicit distinct version2 freeze variants required')
    params=d.get('params');parsed=mechanism(r);profile=pins()['sources'].get(r['name'])
    if not isinstance(params,dict) or set(params)!={'schema','mechanism'} or type(params['schema']) is not int or params['schema']!=1:
        raise ValueError('explicit freeze schema required')
    if not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not exact_pin(params['mechanism'],parsed):
        raise ValueError('complete Aura cost/type/Oracle/predicate pin changed')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],mana=cost_from_oracle(r))


def eligible(r,p,*,power=None,flying=None,colors=None):
    q=p['predicate'];types=set(r['type_line'].partition(' — ')[0].split())
    if not set(q['types']) & types:return False
    if q['power_max'] is not None and (int(r['power']) if power is None else power)>q['power_max']:return False
    if q['colors'] and not set(q['colors']) & set(r.get('colors',[]) if colors is None else colors):return False
    if q['without_flying'] and ('Flying' in r.get('keywords',[]) if flying is None else flying):return False
    return True


def variants(p):
    modes=['tap','already_tapped','payment','insufficient_mana','wrong_color','countered','own_host','foreign_owned_host',
           'cleanup','first_second_untap','aura_destroyed','aura_bounced','host_destroyed','host_bounced','bounce_recast',
           'manual_untap','mana_function','attack_after_untap','ready_snapshot','pending_snapshot','attached_snapshot','resolved_snapshot',
           'nonmatching_entry','no_legal_host','shroud_host','host_bounced_pending']
    if p['flash']:modes.append('offturn')
    if p['predicate']['power_max'] is not None:modes+=['power_grows','pending_illegal']
    if p['predicate']['without_flying']:modes+=['gains_flying','pending_illegal']
    if p['predicate']['colors']:modes+=['color_changes','pending_illegal']
    modes += [f'host_{i}' for i in range(len(PALETTE))]
    return [f'{m}_{owner}' for owner in (0,1) for m in modes]


def validate_witnesses(oracle,rows):
    for n,r in pins()['witnesses'].items():
        if not exact_pin(r,oracle.get(n)) or n not in rows or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL') or (rows[n]['handler']=='card_dummy' and not re.fullmatch(r'(?:Flying|Reach|Defender)?(?: \(.*\))?',r['oracle_text'])):
            raise ValueError('freeze witness pin/implementation changed: '+n)
    for n,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(n)) or rows[n]['csvid']!=p['csvid'] or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL','card_dummy'):
            raise ValueError('freeze normal source identity/implementation changed: '+n)


class FreezeLedger(SelectionLedger):
    def __init__(self,case,p,mode,owner,rows,oracle):
        super().__init__(case,p,mode,owner,rows,oracle)
        for item in case['setup']:
            self.entities[item['ref']].update(owner=1-item['player'] if 'opponent_owned' in item.get('state',[]) else item['player'],
                tapped='tapped' in item.get('state',[]),boost=0,extra_flying=False,colors=None)

    def move(self,ref,destination):
        e=self.entities[ref];name=e['name'];pl=e['player']
        if e['zone'] in self.zones:self.zones[e['zone']][pl].remove(name)
        if destination in self.zones:pl=e['owner'];self.zones[destination][pl].append(name)
        e.update(zone=destination,player=pl,tapped=False,boost=0,extra_flying=False,colors=None)


def _case(name,csvid,d,p,variant,rows,oracle):
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    target='Elvish Mystic'
    if mode.startswith('host_') and mode.split('_')[1].isdigit():target=PALETTE[int(mode.split('_')[1])]
    if mode=='no_legal_host':target='Forest'
    if mode=='shroud_host':target='Calcite Snapper'
    host_player=owner if mode in ('own_host','foreign_owned_host') else 1-owner
    valid=mode!='shroud_host' and eligible(oracle[target],p)
    def add(n,ref,pl,zone='battlefield',tapped=False,foreign=False):
        item=card(n,ref,pl,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(item)
    add(name,'source',owner,'hand');add(target,'host',host_player,tapped=mode=='already_tapped',foreign=mode=='foreign_owned_host')
    if valid:add('Grizzly Bears','unselected',host_player,tapped=True)
    for pl in (0,1):
        add('Seal of Cleansing',f'control_{pl}',pl)
        add('Plains',f'hand_{pl}',pl,'hand');add('Mountain',f'grave_{pl}',pl,'graveyard');add('Island',f'exile_{pl}',pl,'exile')
        for i,n in enumerate(['Forest','Swamp','Hill Giant']):add(n,f'library_{pl}_{i}',pl,'library')
    if mode in ('aura_bounced','host_bounced','bounce_recast','host_bounced_pending','foreign_owned_host'):add('Boomerang','bounce',owner,'hand')
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode in ('manual_untap','mana_function','attack_after_untap'):add('Twiddle','untapper',owner,'hand')
    if mode=='mana_function':add('Fog','mana_spender',host_player,'hand')
    if mode=='manual_untap':add('Twiddle','retapper',owner,'hand')
    modifier='Giant Growth' if p['predicate']['power_max'] is not None else 'Jump' if p['predicate']['without_flying'] else 'Purelace'
    if mode=='pending_illegal' and p['predicate']['colors']:modifier='Turn to Frog'
    if mode in ('power_grows','gains_flying','color_changes','pending_illegal'):add(modifier,'modifier',owner,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=FreezeLedger(case,p,mode,owner,rows,oracle)
    actions=[];pools=[[0]*6,[0]*6];life=[20,20];attached=False;current=1-owner if mode=='offturn' else owner
    actions.append(step('turn',player=current,value=0))
    if mode in ('manual_untap','mana_function','attack_after_untap'):actions.append(step('present_ai_dialogs',value=1))
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(pl,vector):pools[pl]=list(vector);actions.extend(step('set_mana',player=pl,color=c,value=n) for c,n in enumerate(vector))
    def cast(ref,response=False):
        e=w.entities[ref];pl=e['player'];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pools[pl]=[a-b for a,b in zip(pools[pl],cost(e['name']))]
        if min(pools[pl])<0:raise ValueError('freeze fixture real typed payment insufficient')
        w.move(ref,'stack');actions.extend([step('assert_stack',value=2 if response else 1),step('assert_card',ref=ref,property='in_hand',value=0)])
    def receipts():
        seq=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=0),step('assert_forced_zone_choices',value=0)]
        universe=sorted({e['name'] for e in w.entities.values()})
        for pl in (0,1):
            seq += [step('assert_life',player=pl,value=life[pl]),step('assert_shuffle_count',player=pl,value=0)]
            if mode!='cleanup':seq += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(pools[pl])]
            for zone in ('battlefield','hand','graveyard','exile','library'):
                seq.append(step('assert_zone',player=pl,zone=zone,value=w.count(pl,zone)))
                for n in universe:
                    count=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in w.entities.values()) if zone=='battlefield' else w.zones[zone][pl].count(n)
                    seq.append(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[n]['csvid'],value=count))
                if zone=='library':seq += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(w.zones[zone][pl])]
            totals=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl]);seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=totals[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])+e['boost']) for k in ('power','toughness')]
                seq += [step('assert_card',ref=ref,property=k,value=0) for k in ('damage','plus1_counters','minus1_counters')]
            if e.get('frog'):
                seq += [step('assert_matching_permanents',player=e['player'],index=rows[e['name']]['csvid'],property=k,value=v) for k,v in [('color',4),('subtype_Frog',1),('subtype_Elf',0)]]
                seq.append(step('require_can_activate',ref=ref,value=0))
        return seq
    def aura_leave(destination='graveyard'):
        nonlocal attached
        w.move('source',destination);attached=False
    def next_untap():
        nonlocal current,pools
        if any(any(vector) for vector in pools):raise ValueError('spend actual produced mana before the zero-mana untap API boundary')
        actions.append(step('advance_turn'));current=1-current;pools=[[0]*6,[0]*6]
        for e in w.entities.values():e['boost']=0;e['extra_flying']=False
        actions.append(step('untap_step',player=current))
        for ref,e in w.entities.items():
            if e['zone']=='battlefield' and e['player']==current and not(attached and ref=='host'):e['tapped']=False
        actions.extend(receipts())
    def own_untap():
        # Always use real later turns, including when host and caster differ.
        next_untap()
        if current!=host_player:next_untap()
    def helper(ref,target_ref='host',response=False,dialog=None):
        e=w.entities[ref];funds(e['player'],cost(e['name']));actions.append(step('choose_target',player=e['player'],ref=target_ref));cast(ref,response)
        if dialog is not None:actions.append(step('choose_dialog',player=e['player'],value=dialog))
        actions.append(step('resolve_response' if response else 'resolve'));w.move(ref,'graveyard')
    def modify():
        e=w.entities['host']
        if modifier=='Giant Growth':e['boost']+=3
        elif modifier=='Jump':e['extra_flying']=True
        elif modifier=='Turn to Frog':e.update(colors=['U'],frog=True)
        else:e['colors']=['W']
    def enter():
        nonlocal attached
        w.move('source','battlefield');attached=True;w.entities['host']['tapped']=True
        actions.append(step('resolve'));actions.extend(receipts())
    funds(0,[0]*6);funds(1,[0]*6);pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':
        c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
    funds(owner,pool)
    if mode=='countered':funds(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready=len(actions)
    if not valid or mode in ('insufficient_mana','wrong_color'):
        # This query dispatches EVENT_CAN_CAST (here, target availability).
        # Actual cast still enforces all mana/payment requirements.
        actions += [step('require_can_cast',ref='source',value=int(valid)),step('cast',ref='source',value=0)];pending=len(actions);actions.extend(receipts())
    else:
        actions.append(step('choose_target',player=owner,ref='host'));cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('counter','graveyard');w.move('source','graveyard');actions.extend(receipts())
        elif mode in ('host_bounced_pending','pending_illegal'):
            actions.append(step('announce_spell',ref='source'))
            if mode=='host_bounced_pending':helper('bounce',response=True);w.move('host','hand')
            else:helper('modifier',response=True);modify()
            actions.append(step('resolve'));aura_leave();actions.extend(receipts())
        else:enter()
    attached_cut=len(actions)
    if mode in ('aura_destroyed','attached_snapshot'):
        actions.append(step('destroy',ref='source'));aura_leave();actions.append(step('resolve'));actions.extend(receipts());own_untap()
    if mode in ('aura_bounced','bounce_recast'):
        helper('bounce','source');aura_leave('hand');actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts())
        if mode=='bounce_recast':
            own_untap();actions.append(step('turn',player=current,value=0))
            if current!=owner:next_untap();actions.append(step('turn',player=current,value=0))
            funds(owner,p['mana']);actions.append(step('choose_target',player=owner,ref='host'));cast('source');enter();own_untap()
        else:own_untap()
    if mode=='host_destroyed':
        actions.append(step('destroy',ref='host'));w.move('host','graveyard');aura_leave();actions.append(step('resolve'));actions.extend(receipts())
    if mode in ('host_bounced','foreign_owned_host'):helper('bounce');w.move('host','hand');aura_leave();actions.extend(receipts())
    if mode in ('power_grows','gains_flying','color_changes'):
        helper('modifier');modify();aura_leave();actions.append(step('refresh'));actions.extend(receipts());own_untap()
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipts());own_untap()
    if mode=='first_second_untap':own_untap();next_untap();own_untap()
    if mode in ('manual_untap','mana_function','attack_after_untap'):
        if mode=='attack_after_untap':own_untap();actions.append(step('turn',player=current,value=0))
        helper('untapper',dialog=1);w.entities['host']['tapped']=False;actions.extend(receipts())
        if mode=='manual_untap':helper('retapper',dialog=0);w.entities['host']['tapped']=True;actions.extend(receipts());own_untap()
        elif mode=='mana_function':
            actions += [step('require_can_activate',ref='host',value=1),step('activate',ref='host',value=1),step('assert_stack',value=0)]
            pools[host_player][3]+=1;w.entities['host']['tapped']=True;actions.extend(receipts())
            # Preserve the real G1 production receipt, then pay that exact
            # mana normally. Do not erase an opponent's pool in bookkeeping.
            cast('mana_spender');actions.append(step('resolve'));w.move('mana_spender','graveyard');actions.extend(receipts());own_untap()
        else:
            actions += [step('begin_attackers'),step('attack',ref='host',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage')]
            w.entities['host']['tapped']=True;life[1-host_player]-=int(oracle[target]['power']);actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    if mode=='nonmatching_entry':funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipts())
    if mode in ('ready_snapshot','pending_snapshot','attached_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending if mode=='pending_snapshot' else attached_cut;tail=copy.deepcopy(actions[cut:]);actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    case.update(name=f'EntryAuraFreezeRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=p['clauses'],
        untested_components=['no generic attachment identity observer: real selected host/effects/source and host departures compose witness','source departure between Aura entry and its automatic ETB resolution: no exposed priority boundary','arbitrary Aura movement/control change/humiliation/phasing','replacement effects/tokens/multiface/multiplayer']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('freeze source identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
