"""Whole-Oracle nontargeted own-return ETBs and their printed callbacks."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from entry_trigger_value_selection_recipes import SelectionLedger
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'entry_own_return'
DATA = Path(__file__).parent / 'data/entry_own_return_wave8_profiles.json'
COLORS = {'white':'W', 'blue':'U', 'black':'B', 'red':'R', 'green':'G'}
SECONDARIES = {
    "Whenever this creature deals combat damage to a player, look at that player's hand and choose a card from it. The player discards that card.":'combat_discard',
    'When this creature enters, each player discards a card.':'discard_each',
    'When this creature enters, draw two cards, then put two cards from your hand on the bottom of your library.':'draw_bottom',
    'When this creature enters, it deals 1 damage to target player or planeswalker.':'damage',
    'When this creature enters, exile target card from a graveyard.':'grave_exile',
    '{T}: Add two mana of any one color.':'mana',
}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(r):
    types = set(r.get('type_line', '').partition(' — ')[0].split()) if r else set()
    if not r or r.get('layout') != 'normal' or r.get('card_faces') or r.get('selected_face') or not types & {'Creature','Artifact'}:
        raise ValueError('own return requires a whole normal Creature or Artifact')
    if r['oracle_text']=="When this creature enters, return each other creature you control to its owner's hand.":
        if 'Creature' not in types or any(not str(r.get(k,'')).isdigit() for k in ('power','toughness')):
            raise ValueError('mass-other return requires its actual fixed-stat Creature')
        return dict(scope='creature',colors=[],count=None,another=True,optional=False,creature=True,flash=False,flying=False,
                    secondary=None,entry_clause=r['oracle_text'],other_clauses=[],mass=True)
    pattern = r"When this (creature|artifact) enters, (you may )?return (a|another|two) (?:(white or blue|blue or black|red or green) )?(permanent|creature|lands) you control to (?:its|their) owner's hand\."
    matches = [(line, re.fullmatch(pattern, line)) for line in r['oracle_text'].splitlines()]
    matches = [(line,m) for line,m in matches if m]
    if len(matches) != 1:
        raise ValueError('one complete nontargeted own-return clause required')
    clause,m = matches[0]
    if m[1].capitalize() not in types or (m[3]=='two') != (m[5]=='lands'):
        raise ValueError('own-return source and quantity grammar disagree')
    other = [line for line in r['oracle_text'].splitlines() if line != clause]
    secondary = []
    for line in other:
        if line in SECONDARIES: secondary.append(SECONDARIES[line])
        elif line == 'Flying' or re.fullmatch(r'Flash(?: \(.*\))?',line): pass
        else: raise ValueError('unmodeled own-return printed clause')
    if len(secondary)>1 or set(r.get('keywords',[]))-{'Flash','Flying'}:
        raise ValueError('unreviewed secondary callback/passive composition')
    creature = 'Creature' in types
    if creature and any(not str(r.get(k,'')).isdigit() for k in ('power','toughness')):
        raise ValueError('fixed printed creature stats required')
    return dict(scope=m[5].removesuffix('s'), colors=[COLORS[c] for c in m[4].split(' or ')] if m[4] else [],
                count=2 if m[3]=='two' else 1, another=m[3]=='another', optional=bool(m[2]),
                creature=creature, flash='Flash' in r.get('keywords',[]), flying='Flying' in r.get('keywords',[]),
                secondary=secondary[0] if secondary else None, entry_clause=clause, other_clauses=other)


def spec(d,r):
    from recipes import cost_from_oracle
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants']):
        raise ValueError('explicit version2 own-return variants required')
    q=d.get('params'); parsed=mechanism(r); profile=pins()['sources'].get(r['name'])
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=1:
        raise ValueError('own-return schema required')
    if not profile or not exact_pin(profile['oracle'],r) or d.get('oracle_text')!=r['oracle_text'] or not exact_pin(q['mechanism'],parsed) or not exact_pin(q['presentation'],profile['presentation']):
        raise ValueError('whole own-return Oracle/mechanism/presentation pin changed')
    if set(q['presentation'])!={'human_optional_trigger'} or type(q['presentation']['human_optional_trigger']) is not bool:
        raise ValueError('actual optional-entry presentation required')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=r['name'],presentation=copy.deepcopy(q['presentation']),mana=cost_from_oracle(r))


def eligible(r,p):
    types=set(r['type_line'].partition(' — ')[0].split())
    return (bool(types & {'Creature','Artifact','Land','Enchantment','Planeswalker'}) if p['scope']=='permanent' else p['scope'].capitalize() in types) and (not p['colors'] or bool(set(p['colors']) & set(r.get('colors',[]))))


def variants(p):
    if p.get('mass'):
        modes=['effect','payment','insufficient_mana','wrong_color','countered','foreign_owned','empty_others','opponent_only',
               'artifact_creature','land_creature','protected_others','bounce_recast','nonmatching_entry',
               'ready_snapshot','pending_snapshot','resolved_snapshot']
        return [f'{m}_{o}' for o in (0,1) for m in modes]
    modes=['effect','payment','insufficient_mana','countered','foreign_owned','alternate_choice','bounce_recast',
           'cleanup','nonmatching_entry','ready_snapshot','pending_snapshot','resolved_snapshot']
    if any(p['mana'][1:]): modes.append('wrong_color')
    if p['optional']: modes += ['decline','no_other','artifact_choice','land_choice','enchantment_choice']
    else: modes.append('mandatory_cancel')
    if p['creature'] and not p['another']: modes.append('source_choice')
    if p['scope']!='land': modes.append('shroud_choice')
    if p['scope']!='land': modes.append('protection_choice')
    if p['colors']: modes += ['other_color','wrong_color_only']
    if p['flash']: modes.append('offturn')
    if p['flying']: modes += ['flying_evasion','flying_reach']
    if p['secondary']=='grave_exile': modes += ['own_grave','opponent_grave','empty_graves','both_graves']
    if p['secondary']=='discard_each': modes += ['empty_hands','last_card_hands']
    if p['secondary']=='draw_bottom': modes += ['exact_library','alternate_bottom']
    if p['secondary']=='damage': modes += ['self_damage','planeswalker_damage']
    if p['secondary']=='mana': modes += ['zero_lands','one_land','three_lands'] + ['mana_'+c for c in 'BUGRW']
    if p['secondary']=='combat_discard':
        modes += ['combat_choice','combat_land','combat_empty','combat_one','combat_cancel','combat_snapshot','noncombat_source']
    result=[f'{m}_{o}' for o in (0,1) for m in modes]
    if p['presentation']['human_optional_trigger']: result.append('whole_decline_0')
    return result


def validate_witnesses(oracle,rows):
    for n,r in pins()['witnesses'].items():
        if not exact_pin(r,oracle.get(n)) or n not in rows or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL') or (rows[n]['handler']=='card_dummy' and not re.fullmatch(r'(?:Flying|Reach|Defender)?(?: \(.*\))?',r['oracle_text'])):
            raise ValueError('own-return witness identity/implementation changed: '+n)
    for n,p in pins()['sources'].items():
        if not exact_pin(p['oracle'],oracle.get(n)) or rows[n]['csvid']!=p['csvid'] or rows[n]['kind']!='card' or rows[n]['handler'] in (None,'NULL','card_dummy'):
            raise ValueError('own-return source identity/implementation changed: '+n)


class ReturnLedger(SelectionLedger):
    """Reuse immutable selection state/counts; preserve original zone owners."""
    def __init__(self,case,p,mode,owner,rows,oracle):
        super().__init__(case,p,mode,owner,rows,oracle)
        self.uncertain_discard=False
        for item in case['setup']:
            self.entities[item['ref']].update(owner=1-item['player'] if 'opponent_owned' in item.get('state',[]) else item['player'],
                tapped='tapped' in item.get('state',[]),damage=0)

    def move(self,ref,destination):
        e=self.entities[ref];old=e['zone'];pl=e['player'];name=e['name']
        if old in self.zones:self.zones[old][pl].remove(name)
        if destination in self.zones:
            pl=e['owner'];self.zones[destination][pl].append(name)
        e.update(zone=destination,player=pl,damage=0,tapped=False)

    def draw(self,player,n):
        if len(self.zones['library'][player])<n:raise ValueError('pilot does not fabricate failed mandatory draws')
        for _ in range(n):self.zones['hand'][player].append(self.zones['library'][player].pop(0))


def _case(name,csvid,d,p,variant,rows,oracle):
    if p.get('mass'):return _mass_case(name,csvid,d,p,variant,rows,oracle)
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    def add(n,ref,pl,zone='battlefield',foreign=False,tapped=False):
        item=card(n,ref,pl,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(item)
    def primary(second=False):
        if p['scope']=='land':return 'Island' if second else 'Forest'
        if mode=='artifact_choice':return 'Darksteel Relic'
        if mode=='land_choice':return 'Forest'
        if mode=='enchantment_choice':return 'Seal of Cleansing'
        if mode=='shroud_choice':return 'Blurred Mongoose' if 'G' in p['colors'] else 'Calcite Snapper'
        if mode=='protection_choice':return 'Progenitus'
        order=['Merfolk of the Pearl Trident','Scathe Zombies','Savannah Lions','Goblin Piker','Grizzly Bears']
        legal=[n for n in order if eligible(oracle[n],p)]
        return legal[-1] if second or mode=='other_color' else legal[0]
    add(name,'source',owner,'hand')
    for pl in (0,1):
        hand_count=0 if mode=='empty_hands' else 1 if mode=='last_card_hands' else 3
        hand_names=['Plains']*hand_count
        if p['secondary']=='combat_discard' and pl==1-owner:
            hand_names=[] if mode=='combat_empty' else ['Forest'] if mode=='combat_one' else ['Plains','Ornithopter','Scathe Zombies']
        for i,n in enumerate(hand_names):add(n,f'padding_{pl}_{i}',pl,'hand')
        deck=['Forest','Island','Mountain','Swamp','Plains','Grizzly Bears','Hill Giant','Forest']
        if mode=='exact_library' and pl==owner:deck=deck[:2]
        for i,n in enumerate(deck):add(n,f'library_{pl}_{i}',pl,'library')
        if p['secondary']!='grave_exile' or mode not in ('empty_graves','own_grave','opponent_grave') or pl==(owner if mode=='own_grave' else 1-owner if mode=='opponent_grave' else -1):
            add('Mountain',f'grave_{pl}_0',pl,'graveyard');add('Plains',f'grave_{pl}_1',pl,'graveyard')
        add('Swamp',f'exile_{pl}',pl,'exile')
    amount=4 if p['scope']=='land' and mode=='bounce_recast' else 0 if mode in ('zero_lands','no_other','source_choice','wrong_color_only') else 1 if mode=='one_land' else 3 if mode=='three_lands' else 2
    if p['scope']!='land':amount=min(amount,1 if mode=='protection_choice' else 2)
    for i in range(amount):
        n=primary(i%2==1)
        if i>1:n=('Mountain','Swamp')[i-2]
        add(n,f'target_{i}',owner,foreign=mode=='foreign_owned' and i==0,tapped=p['scope']=='land')
    if mode=='wrong_color_only':add('Ornithopter','wrong_type_or_color',owner)
    # Opponent's otherwise matching permanent is always excluded by control.
    add(primary(),'opponent_only',1-owner,tapped=p['scope']=='land')
    if mode=='bounce_recast':add('Temporal Adept','bouncer',owner)
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Darksteel Relic','nonmatching',owner,'hand')
    if mode in ('flying_evasion','flying_reach'):add('Giant Spider' if mode=='flying_reach' else 'Elvish Mystic','blocker',1-owner)
    if mode=='planeswalker_damage':add('Jace Beleren','walker',1-owner,'hand')
    if mode=='noncombat_source':add('Hermetic Study','grant',owner,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=ReturnLedger(case,p,mode,owner,rows,oracle)
    actions=[step('turn',player=1-owner if mode=='offturn' else owner,value=0),step('discard_policy',value=1)]
    pools=[[0]*6,[0]*6];life=[20,20];forced=0;loyalty=None;forced_zones=0
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def funds(pl,vector):pools[pl]=list(vector);actions.extend(step('set_mana',player=pl,color=c,value=n) for c,n in enumerate(vector))
    def pay(pl,vector):
        pools[pl]=[a-b for a,b in zip(pools[pl],vector)]
        if min(pools[pl])<0:raise ValueError('negative real payment ledger')
    def receipts():
        seq=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=forced),step('assert_forced_zone_choices',value=forced_zones)]
        universe=sorted({e['name'] for e in w.entities.values()})
        for pl in (0,1):
            seq += [step('assert_life',player=pl,value=life[pl]),step('assert_shuffle_count',player=pl,value=0)]
            if mode!='cleanup':seq += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(pools[pl])]
            for zone in ('battlefield','hand','graveyard','exile','library'):
                seq.append(step('assert_zone',player=pl,zone=zone,value=w.count(pl,zone)))
                for n in universe:
                    if pl==1 and w.uncertain_discard and zone in ('hand','graveyard'):continue
                    count=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in w.entities.values()) if zone=='battlefield' else w.zones[zone][pl].count(n)
                    seq.append(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[n]['csvid'],value=count))
                if zone=='library':seq += [step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(w.zones[zone][pl])]
            counts=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl])
            seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=counts[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])) for k in ('power','toughness')]
                seq += [step('assert_card',ref=ref,property='damage',value=e['damage'])]
                seq += [step('assert_card',ref=ref,property=k,value=0) for k in ('plus1_counters','minus1_counters')]
        if loyalty is not None:seq.append(step('assert_counter',ref='walker',counter_kind='loyalty',value=loyalty))
        return seq
    def cast(ref,response=False):
        e=w.entities[ref];actions.append(step('cast_response' if response else 'cast',ref=ref,value=1));pay(e['player'],cost(e['name']));w.move(ref,'stack')
        actions.extend([step('assert_card',ref=ref,property='in_hand',value=0),step('assert_stack',value=2 if response else 1)])
    def discard_each():
        for pl in (0,1):
            hand=w.zones['hand'][pl]
            if not hand:continue
            # AI may choose directly without a selector request. Its identity
            # remains honest through hand+grave totals and exact zone counts.
            if pl==1:w.uncertain_discard=True
            chosen=min(hand,key=lambda n:rows[n]['csvid']);hand.remove(chosen);w.zones['graveyard'][pl].append(chosen)
    def secondary(kind):
        nonlocal loyalty
        if kind=='discard_each':discard_each()
        elif kind=='draw_bottom':
            w.draw(owner,2);available=list(w.zones['hand'][owner])
            chosen=list(reversed(available))[:2] if mode=='alternate_bottom' else available[:2]
            for n in chosen:
                actions.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']));w.zones['hand'][owner].remove(n);w.zones['library'][owner].append(n)
        elif kind=='damage':
            if mode=='planeswalker_damage':actions.append(step('choose_target',player=owner,ref='walker'));loyalty-=1
            else:
                pl=owner if mode=='self_damage' else 1-owner
                actions.append(step('choose_player',player=owner,value=pl));life[pl]-=1
        elif kind=='grave_exile':
            legal=[pl for pl in (0,1) if w.zones['graveyard'][pl]]
            if not legal:return
            pl=owner if mode=='own_grave' else 1-owner if 1-owner in legal else owner
            if len(legal)==2:actions.append(step('choose_player',player=owner,value=pl))
            n=w.zones['graveyard'][pl][0]
            actions.append(step('choose_zone_card',player=owner,value=rows[n]['csvid']));w.zones['graveyard'][pl].remove(n);w.zones['exile'][pl].append(n)
    def effect():
        nonlocal forced
        if mode=='whole_decline':return
        if owner==0 and p['presentation']['human_optional_trigger']:actions.append(step('choose_trigger',player=owner,ref='source'))
        # These cards have independent printed triggers; the legacy handler
        # composes them in this offered order. Alternative ordering is a gap.
        if p['secondary']=='damage':secondary('damage')
        prior_empty=not w.zones['hand'][owner]
        if p['secondary']=='discard_each' and prior_empty:secondary('discard_each')
        legal=[ref for ref,e in w.entities.items() if e['zone']=='battlefield' and e['player']==owner and eligible(oracle[e['name']],p) and not(p['another'] and ref=='source')]
        if mode in ('source_choice','wrong_color_only'):legal.sort(key=lambda ref:ref!='source')
        elif mode=='alternate_choice':legal.sort(key=lambda ref:ref!='target_1')
        else:legal.sort(key=lambda ref:(not ref.startswith('target_'),ref))
        selected=legal[:p['count']]
        if mode=='decline':
            if not selected:raise ValueError('decline control requires ordinary legal fallback')
            actions.append(step('choose_optional_target',player=owner,ref=selected[0]));selected=[]
        else:
            for ref in selected:
                actions.append(step('choose_optional_target' if mode=='mandatory_cancel' else 'choose_target',player=owner,ref=ref))
                if mode=='mandatory_cancel':forced+=1
        for ref in selected:w.move(ref,'hand')
        if p['secondary']=='discard_each' and not prior_empty:secondary('discard_each')
        if p['secondary'] in ('draw_bottom','grave_exile'):secondary(p['secondary'])
    def enter():
        w.move('source','battlefield');effect();actions.append(step('resolve'));actions.extend(receipts())
    funds(0,[0]*6);funds(1,[0]*6)
    if mode=='planeswalker_damage':
        # A real planeswalker cast establishes its actual starting loyalty.
        actions.append(step('turn',player=1-owner,value=0));funds(1-owner,cost('Jace Beleren'));cast('walker');actions.append(step('resolve'));w.move('walker','battlefield');loyalty=3
        actions.append(step('advance_turn'));pools=[[0]*6,[0]*6];actions.extend(receipts())
    pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':
        c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
    funds(owner,pool)
    if mode=='countered':funds(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready=len(actions)
    if mode in ('insufficient_mana','wrong_color'):
        actions.append(step('cast',ref='source',value=0));pending=len(actions);actions.extend(receipts())
    else:
        cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('counter','graveyard');w.move('source','graveyard');actions.extend(receipts())
        else:enter()
    if mode=='bounce_recast':
        funds(owner,[0,0,3,0,0,0]);actions += [step('require_can_activate',ref='bouncer',value=1),step('choose_target',player=owner,ref='source'),step('activate',ref='bouncer',value=1)]
        pay(owner,[0,0,3,0,0,0]);w.entities['bouncer']['tapped']=True;actions.append(step('resolve'));w.move('source','hand')
        actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts());funds(owner,p['mana']);cast('source');enter()
    if mode=='nonmatching_entry':
        funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipts())
    if p['secondary']=='mana' and (mode.startswith('mana_') or mode in ('effect','one_land','zero_lands')):
        c={'B':1,'U':2,'G':3,'R':4,'W':5}.get(mode[-1],3)
        actions += [step('require_can_activate',ref='source',value=1),step('choose_color',player=owner,value=c),step('activate',ref='source',value=1),step('assert_stack',value=0)]
        pools[owner][c]+=2;w.entities['source']['tapped']=True;actions.extend(receipts());actions.append(step('require_can_activate',ref='source',value=0))
    if mode=='cleanup':actions.append(step('cleanup'));actions.extend(receipts())
    combat_cut=None
    if mode in ('flying_evasion','flying_reach','combat_choice','combat_land','combat_empty','combat_one','combat_cancel','combat_snapshot','noncombat_source'):
        for pl in (1-owner,owner):
            actions += [step('advance_turn'),step('untap_step',player=pl)];pools=[[0]*6,[0]*6]
            for e in w.entities.values():
                if e['zone']=='battlefield' and e['player']==pl:e['tapped']=False
        actions.append(step('turn',player=owner,value=0));combat_cut=len(actions)
        if mode=='noncombat_source':
            funds(owner,cost('Hermetic Study'));actions.append(step('choose_target',player=owner,ref='source'));cast('grant');actions.append(step('resolve'));w.move('grant','battlefield')
            actions += [step('require_can_activate',ref='grant',value=1),step('choose_player',player=owner,value=1-owner),step('activate',ref='grant',value=1),step('resolve')]
            w.entities['source']['tapped']=True;life[1-owner]-=1;actions.extend(receipts())
        else:
            actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers')]
            if mode in ('flying_evasion','flying_reach'):actions.append(step('block',ref='blocker',other='source',value=int(mode=='flying_reach')))
            actions += [step('advance_combat'),step('assert_card',ref='source',property='blocked',value=int(mode=='flying_reach'))]
            if p['secondary']=='combat_discard' and mode!='flying_reach':
                available=w.zones['hand'][1-owner]
                if available:
                    chosen='Plains' if mode=='combat_land' else 'Ornithopter' if mode in ('combat_choice','combat_snapshot') else available[-1]
                    actions.append(step('choose_optional_zone' if mode=='combat_cancel' else 'choose_zone_card',player=owner,value=rows[chosen]['csvid']))
                    if mode=='combat_cancel':forced_zones+=1
                    available.remove(chosen);w.zones['graveyard'][1-owner].append(chosen)
            actions.append(step('combat_damage'));w.entities['source']['tapped']=True
            if mode=='flying_reach':
                if int(oracle[name]['toughness'])<=2:w.move('source','graveyard')
                else:w.entities['source']['damage']=2
                w.entities['blocker']['damage']=int(oracle[name]['power'])
            else:life[1-owner]-=int(oracle[name]['power'])
            if p['secondary']=='combat_discard':actions.append(step('resolve'))
            actions.extend(receipts());actions.append(step('end_combat'));actions.extend(receipts())
    if mode in ('ready_snapshot','pending_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending;tail=copy.deepcopy(actions[cut:])
        actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    if mode=='combat_snapshot':
        tail=copy.deepcopy(actions[combat_cut:]);actions=actions[:combat_cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    case.update(name=f'EntryOwnReturnRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=[p['entry_clause']]+p['other_clauses'],
        untested_components=['independent simultaneous ETB trigger ordering: legacy callbacks merge; no alternative-order control','source departure between entry and automatic ETB resolution: unavailable priority boundary','generic became-target event observer for nontargeted selectors','mandatory draw beyond available library and loss/replacement effects','AI discard exact chosen identity when no selector is offered: exact counts and per-card hand+grave conservation retained','tokens/multiface/control changes/multiplayer']))
    if p['secondary']=='combat_discard':case['_recipe']['untested_components'] += ['planeswalker attack declaration boundary: current attack API selects defending player only','source departure between combat damage and its automatic trigger: unavailable priority boundary']
    return case


def _mass_case(name,csvid,d,p,variant,rows,oracle):
    """Each OTHER controlled creature returns, with no target-selection UI."""
    from recipes import card,step,cost_from_oracle
    mode,owner=variant.rsplit('_',1);owner=int(owner);setup=[]
    def add(n,ref,pl,zone='battlefield',foreign=False,tapped=True):
        item=card(n,ref,pl,zone)
        if zone=='battlefield':item['state']=['tapped' if tapped else 'untapped']+(['opponent_owned'] if foreign else [])
        setup.append(item)
    add(name,'source',owner,'hand')
    for pl in (0,1):
        add('Plains',f'hand_{pl}',pl,'hand');add('Mountain',f'grave_{pl}',pl,'graveyard');add('Swamp',f'exile_{pl}',pl,'exile')
        for i,n in enumerate(['Forest','Island','Plains','Grizzly Bears']):add(n,f'library_{pl}_{i}',pl,'library')
        add('Forest' if pl==owner else 'Island',f'land_{pl}',pl)
        add('Darksteel Relic',f'artifact_{pl}',pl,tapped=False)
    if mode not in ('empty_others','opponent_only'):
        own=['Progenitus','Calcite Snapper'] if mode=='protected_others' else ['Ornithopter','Scathe Zombies'] if mode=='artifact_creature' else ['Dryad Arbor','Scathe Zombies'] if mode=='land_creature' else ['Elvish Mystic','Scathe Zombies']
        for i,n in enumerate(own):add(n,f'friend_{i}',owner,foreign=mode=='foreign_owned' and i==0)
    add('Elvish Mystic','enemy_0',1-owner)
    if mode=='opponent_only':add('Scathe Zombies','enemy_1',1-owner)
    if mode=='bounce_recast':add('Boomerang','bounce',owner,'hand')
    if mode=='countered':add('Counterspell','counter',1-owner,'hand')
    if mode=='nonmatching_entry':add('Ornithopter','nonmatching',owner,'hand')
    case=dict(card=name,fixture='empty_main',seed=123,setup=setup);w=ReturnLedger(case,p,mode,owner,rows,oracle)
    actions=[step('turn',player=owner,value=0)];pools=[[0]*6,[0]*6]
    def funds(pl,vector):pools[pl]=list(vector);actions.extend(step('set_mana',player=pl,color=c,value=n) for c,n in enumerate(vector))
    def cost(n):return [0]*6 if oracle[n]['mana_cost']=='{0}' else cost_from_oracle(oracle[n])
    def cast(ref,response=False):
        e=w.entities[ref];pl=e['player'];pools[pl]=[a-b for a,b in zip(pools[pl],cost(e['name']))]
        if min(pools[pl])<0:raise ValueError('mass return real payment insufficient')
        actions.extend([step('cast_response' if response else 'cast',ref=ref,value=1),step('assert_stack',value=2 if response else 1),step('assert_card',ref=ref,property='in_hand',value=0)])
        w.move(ref,'stack')
    def receipts():
        seq=[step('assert_choices_consumed'),step('assert_stack',value=0),step('assert_forced_target_choices',value=0),step('assert_forced_zone_choices',value=0)]
        universe=sorted({e['name'] for e in w.entities.values()})
        for pl in (0,1):
            seq += [step('assert_life',player=pl,value=20),step('assert_shuffle_count',player=pl,value=0)]
            seq += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(pools[pl])]
            for zone in ('battlefield','hand','graveyard','exile','library'):
                seq.append(step('assert_zone',player=pl,zone=zone,value=w.count(pl,zone)))
                for n in universe:
                    count=sum(e['zone']=='battlefield' and e['player']==pl and e['name']==n for e in w.entities.values()) if zone=='battlefield' else w.zones[zone][pl].count(n)
                    seq.append(step('assert_zone_multiplicity',player=pl,zone=zone,index=rows[n]['csvid'],value=count))
                if zone=='library':seq.extend(step('assert_zone_card',player=pl,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(w.zones[zone][pl]))
            totals=Counter(w.zones['hand'][pl]+w.zones['graveyard'][pl])
            seq += [step('assert_hand_graveyard_total',player=pl,index=rows[n]['csvid'],value=totals[n]) for n in universe]
        for ref,e in w.entities.items():
            if e['zone']!='battlefield':continue
            seq += [step('assert_card',ref=ref,property='in_play',value=1),step('assert_card',ref=ref,property='tapped',value=int(e['tapped']))]
            if 'Creature' in oracle[e['name']]['type_line'].partition(' — ')[0].split():
                seq += [step('assert_card',ref=ref,property=k,value=int(oracle[e['name']][k])) for k in ('power','toughness')]
                seq += [step('assert_card',ref=ref,property=k,value=0) for k in ('damage','plus1_counters','minus1_counters')]
        return seq
    def enter():
        w.move('source','battlefield')
        for ref,e in list(w.entities.items()):
            if ref!='source' and e['zone']=='battlefield' and e['player']==owner and eligible(oracle[e['name']],p):w.move(ref,'hand')
        actions.append(step('resolve'));actions.extend(receipts())
    funds(0,[0]*6);funds(1,[0]*6);pool=list(p['mana'])
    if mode=='insufficient_mana':pool=[0]*6
    if mode=='wrong_color':pool=[6,0,0,0,0,2]
    funds(owner,pool)
    if mode=='countered':funds(1-owner,cost('Counterspell'))
    actions.append(step('refresh'));ready=len(actions)
    if mode in ('insufficient_mana','wrong_color'):
        actions.append(step('cast',ref='source',value=0));pending=len(actions);actions.extend(receipts())
    else:
        cast('source');actions.append(step('assert_zone',player=owner,zone='hand',value=w.count(owner,'hand')+1));pending=len(actions)
        if mode=='countered':
            actions.append(step('announce_spell',ref='source'));cast('counter',True);actions.append(step('resolve_response'));w.move('source','graveyard');w.move('counter','graveyard');actions.extend(receipts())
        else:enter()
    if mode=='bounce_recast':
        # Recast an actually returned creature, then the actual source. The
        # second ETB returns its fresh object, never a stale hand-slot ref.
        actions.append(step('bind_hand_copy',player=owner,value=rows['Elvish Mystic']['csvid'],index=0,ref='friend_0'))
        funds(owner,cost('Elvish Mystic'));cast('friend_0');actions.append(step('resolve'));w.move('friend_0','battlefield');actions.extend(receipts())
        funds(owner,cost('Boomerang'));actions.append(step('choose_target',player=owner,ref='source'));cast('bounce');actions.append(step('resolve'));w.move('bounce','graveyard');w.move('source','hand')
        actions.append(step('bind_hand_copy',player=owner,value=csvid,index=0,ref='source'));actions.extend(receipts());funds(owner,p['mana']);cast('source');enter()
    if mode=='nonmatching_entry':
        funds(owner,[0]*6);cast('nonmatching');actions.append(step('resolve'));w.move('nonmatching','battlefield');actions.extend(receipts())
    if mode in ('ready_snapshot','pending_snapshot'):
        cut=ready if mode=='ready_snapshot' else pending;tail=copy.deepcopy(actions[cut:])
        actions=actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
    if mode=='resolved_snapshot':actions += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
    case.update(name=f'EntryOwnReturnRecipe_{csvid}_{variant}',steps=actions,_recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],covered_clauses=[p['entry_clause']],
        untested_components=['source departure between entry and automatic ETB resolution: unavailable priority boundary','generic became-target observer: this mass effect never selects targets','token return/zone replacement/control changes/multiplayer']))
    return case


def expand(name,csvid,d,p,selected,rows,oracle):
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('own-return source identity/variants changed')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,d,p,v,rows,oracle) for v in selected]
