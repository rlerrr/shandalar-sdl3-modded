"""Fixed token event components; expectations come from creator Oracle text."""
import copy
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

from token_trigger_creation_spec import spec, variants, validate_witnesses, TOKEN_IDENTITIES, token_identity

def expand(name,csvid,d,p,selected,rows,o):
    if p.get('_wave6_component'):return expand_wave6(name,csvid,d,p,selected,rows,o)
    if p.get("_inspired_component"):return expand_inspired(name,csvid,d,p,selected,rows,o)
    if p.get("_death_watch_component"):return expand_death_watch(name,csvid,d,p,selected,rows,o)
    if p.get('_variable_component'):return expand_variable_component(name,csvid,d,p,selected,rows,o)
    if p.get('_event_component'):return expand_event_component(name,csvid,d,p,selected,rows,o)
    if p.get('_entry_component'):return expand_entry_component(name,csvid,d,p,selected,rows,o)
    from recipes import card,step,observe,cost_from_oracle
    result=[];t=p['token'];token_name=token_identity(p);token_id=rows[token_name]['csvid']
    for requested in selected:
        v,owner=requested.rsplit('_',1);owner=int(owner);bare=v=='bare_zones'
        hands=[[] if bare else list(HAND) for _ in (0,1)];graves=[[] if bare else list(GRAVE) for _ in (0,1)]
        libs=[[] if bare else list(LIBRARY) for _ in (0,1)];exiles=[[] if bare else list(EXILE) for _ in (0,1)]
        mana=[[10]*6,[10]*6];life=[20,20];setup=[];entities={};count=0;token_tapped=False;token_damage=0
        pre=[step('turn',player=owner,value=0)];actions=[]
        def add(nm,ref,player,zone='hand'):
            setup.append(card(nm,ref,player,zone));r=o[nm];creature='Creature' in r['type_line']
            entities[ref]=dict(name=nm,player=player,alive=zone=='battlefield',creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0,damage=0,tapped=False)
            if zone=='hand':hands[player].append(nm)
        def pay(player,cost):mana[player]=[a-b for a,b in zip(mana[player],cost)]
        def leave(ref,zone):
            e=entities[ref];e['alive']=False;{'hand':hands,'graveyard':graves}[zone][e['player']].append(e['name'])
        def normal_cast_choices():
            # Presentation binding only: the printed Dash alternative remains
            # an inactive gap. Its normal-cast option is offered when affordable.
            return [step('choose_dialog',player=owner,value=1)] if name=='Mardu Strike Leader' and sum(mana[owner])>=4 and mana[owner][1]>=1 else []
        def source_cast(ref='source'):
            nonlocal count
            choices=normal_cast_choices();hands[owner].remove(name);pay(owner,p['mana']);e=entities[ref]
            if p['creature']:e['alive']=True
            else:graves[owner].append(name)
            if p['mode']=='entry_attack' or p.get('_entry_death'):count+=t['count']
            return choices+[step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
        def spell(nm,ref,target=None):
            caster=1-owner if p['mode']=='own_spell' else owner
            add(nm,ref,caster);hands[caster].remove(nm);graves[caster].append(nm);pay(caster,cost_from_oracle(o[nm]))
            return ([step('choose_target',player=caster,ref=target)] if target else [])+([step('choose_dialog',player=caster,value=0)] if nm=='Twiddle' else [])+[step('cast',ref=ref,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
        add(name,'source',owner);add('Grizzly Bears','unselected',owner,'battlefield')
        if p.get('_landfall'):
            land_owner=1-owner if v=='opponent_land' else owner
            add('Walking Atlas','atlas',land_owner,'battlefield');add('Forest','trigger_land',land_owner)
        if v=='two_casts':add(name,'second',owner)
        if v in ('payment','insufficient_mana','wrong_color'):
            pool=list(p['mana']) if v!='insufficient_mana' else [0]*6
            if v=='wrong_color':
                c=next(c for c in range(1,6) if pool[c]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
            mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
        if v in ('insufficient_mana','wrong_color'):
            actions += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
        elif v=='countered':
            choices=normal_cast_choices();add('Counterspell','counter',0);hands[owner].remove(name);hands[0].remove('Counterspell');graves[owner].append(name);graves[0].append('Counterspell');pay(owner,p['mana']);pay(0,cost_from_oracle(o['Counterspell']))
            actions += choices+[step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
        else:actions += source_cast()
        failed=v in ('insufficient_mana','wrong_color','countered')
        death=p['mode']=='death';effect_cut=len(actions)
        if not failed:
            actions += [step('assert_zone_multiplicity',player=owner,zone='battlefield',index=token_id,value=count)]
            if v=='other_creature':
                add('Grizzly Bears','other',owner);hands[owner].remove('Grizzly Bears');pay(owner,cost_from_oracle(o['Grizzly Bears']));entities['other']['alive']=True
                actions += [step('cast',ref='other',value=1),step('resolve'),step('destroy',ref='other'),step('refresh')];leave('other','graveyard')
            if v in ('bounce_only','bounce_recast'):
                actions+=spell('Unsummon','bounce','source');leave('source','hand');actions += [step('bind_hand',player=owner,value=csvid,ref='source')]
                if v=='bounce_recast':actions+=source_cast()
            if v in ('blink_only','blink_destroy'):
                add('Cloudshift','blink',owner);hands[owner].remove('Cloudshift');graves[owner].append('Cloudshift');pay(owner,cost_from_oracle(o['Cloudshift']))
                actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='blink',value=1),step('resolve'),step('refresh'),step('bind_permanent',player=owner,value=csvid,ref='source')]
                if p['mode']=='entry_attack' or p.get('_entry_death'):count+=t['count']
            if v=='exile_source':
                actions+=spell('Swords to Plowshares','exile','source');entities['source']['alive']=False;exiles[owner].append(name);life[owner]+=p['power']
            skip=v in ('cast_only','other_creature','bounce_only','blink_only','exile_source') or (v=='payment' and not death)
            if death and not skip and not (p.get('_entry_death') and v=='wrath'):
                effect_cut=len(actions)
                if v=='combat_death':
                    add('Scaled Wurm','death_attacker',1-owner,'battlefield')
                    actions += [step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='death_attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='death_attacker',value=1),step('advance_combat'),step('combat_damage'),step('resolve'),step('refresh'),step('end_combat')]
                    entities['death_attacker']['tapped']=True
                    if p['power']>=6 or 'Deathtouch' in p['keywords']:leave('death_attacker','graveyard')
                    else:entities['death_attacker']['damage']=p['power']
                elif v=='murder':actions+=spell('Murder','kill','source')
                elif v=='lethal_damage':actions+=spell('Lightning Bolt','kill','source')
                else:actions += [step('destroy',ref='source'),step('refresh')]
                leave('source','graveyard');count+=t['count']
            elif p['mode']=='activation' and not skip:
                c=p['activation'];pool=c['mana']
                legal=v not in ('activation_insufficient','activation_wrong_color','fresh_denied','tapped_denied')
                if c['tap'] and v!='fresh_denied':actions += [step('advance_turn'),step('advance_turn')]
                if v in ('activation_payment','activation_insufficient','activation_wrong_color'):
                    mana[owner]=list(pool) if v!='activation_insufficient' else [0]*6
                    if v=='activation_wrong_color':
                        k=next(k for k in range(1,6) if mana[owner][k]);mana[owner][1 if k!=1 else 2]+=mana[owner][k];mana[owner][k]=0
                    actions += [step('set_mana',player=owner,color=k,value=n) for k,n in enumerate(mana[owner])]
                if v=='tapped_denied':
                    actions+=spell('Twiddle','tap_source','source');entities['source']['tapped']=True
                effect_cut=len(actions)
                def activate():
                    nonlocal count
                    seq=[step('choose_dialog',player=owner,value=n) for n in d.get('activation_dialogs',{}).get(str(owner),[])]
                    seq += [step('activate',ref='source',value=1),step('assert_stack',value=1)]
                    pay(owner,pool)
                    if c['tap']:entities['source']['tapped']=True
                    if c['sacrifice']:leave('source','graveyard')
                    seq += [step('assert_zone_multiplicity',player=owner,zone='battlefield',index=token_id,value=count),step('resolve'),step('refresh'),step('assert_choices_consumed')]
                    count+=t['count'];return seq
                if legal:
                    actions+=activate()
                    if v=='repeat_activation':actions+=activate()
                    if v=='repeat_denied':actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
                else:actions += [step('require_can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
            elif p['mode'] in ('attack','entry_attack') and not skip:
                actions += [step('advance_turn'),step('advance_turn')];effect_cut=len(actions)
                actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('resolve'),step('refresh'),step('remove_from_combat',ref='source'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]
                entities['source']['tapped']='Vigilance' not in p['keywords'];count+=t['count']
            elif p['mode'] in ('own_spell','opponent_spell') and not skip:
                effect_cut=len(actions);caster=owner if p['mode']=='own_spell' else 1-owner
                add('Unsummon','trigger_spell',caster);hands[caster].remove('Unsummon');graves[caster].append('Unsummon');pay(caster,cost_from_oracle(o['Unsummon']))
                actions += [step('choose_target',player=caster,ref='unselected'),step('cast',ref='trigger_spell',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
                leave('unselected','hand');count+=t['count']
            elif p.get('_landfall') and not skip:
                if v=='source_before_land':actions += [step('destroy',ref='source'),step('refresh')];leave('source','graveyard')
                actions += [step('require_can_activate',ref='atlas',value=1),step('activate',ref='atlas',value=1),step('assert_stack',value=1),step('choose_zone_card',player=land_owner,value=rows['Forest']['csvid'])]
                if (p['optional'] or p.get('_landfall_functional_priority') and v!='create') and owner==0 and land_owner==owner and v not in ('land_decline','source_before_land'):actions += [step('choose_trigger',player=owner,ref='source')]
                actions += [step('resolve'),step('refresh'),step('assert_choices_consumed'),step('bind_permanent',player=land_owner,value=rows['Forest']['csvid'],ref='trigger_land')]
                entities['atlas']['tapped']=True;entities['trigger_land']['alive']=True;hands[land_owner].remove('Forest')
                if land_owner==owner and v not in ('land_decline','source_before_land'):count+=t['count']
            if v=='two_casts':
                actions+=source_cast('second')
                if death:actions += [step('destroy',ref='second'),step('refresh')];leave('second','graveyard');count+=t['count']
            if v=='source_loss' and entities['source']['alive']:
                actions += [step('destroy',ref='source'),step('refresh')];leave('source','graveyard')
        if count:
            # Prove the positive receipt before cleanup or token removal can mask it.
            actions += [step('assert_zone_multiplicity',player=owner,zone='battlefield',index=token_id,value=count)]
            actions += [step('assert_matching_permanents',player=owner,index=token_id,property=k,value=value) for k,value in (('token',1),('power',t['power']),('toughness',t['toughness']))]
        if v=='cleanup':actions += [step('cleanup'),step('refresh')]
        token_index=count-1 if v=='fresh_attack' and p['mode']=='entry_attack' else 0
        if count and v in ('fresh_attack','ready_attack','block_ground','block_flying','evasion','reach_blocker','trample','destroy_token','bounce_token','exile_token'):
            actions += [step('bind_permanent_copy',player=owner,value=token_id,index=token_index,ref='token')]
        if v=='destroy_token':actions += [step('destroy',ref='token'),step('refresh')];count-=1
        if v=='bounce_token':actions+=spell('Unsummon','bounce','token');count-=1
        if v=='exile_token':actions+=spell('Swords to Plowshares','exile','token');life[owner]+=t['power'];count-=1
        if v=='wrath':
            new_death_tokens=t['count'] if p.get('_entry_death') and entities['source']['alive'] else 0
            actions+=spell('Wrath of God','wrath');count=new_death_tokens
            for ref,e in entities.items():
                if e['alive'] and e['creature']:leave(ref,'graveyard')
        combat=v in ('fresh_attack','ready_attack','block_ground','block_flying','evasion','reach_blocker','trample')
        if combat:
            defending=v in ('block_ground','block_flying');keys=set(t['keywords'])
            if not defending and v!='fresh_attack':
                actions += [step('advance_turn'),step('advance_turn')]
            witness='Suntail Hawk' if v=='block_flying' else 'Giant Spider' if v=='reach_blocker' else 'Grizzly Bears' if defending or v in ('evasion','trample') else None
            if witness:add(witness,'witness',1-owner,'battlefield')
            attacker='witness' if defending else 'token';blocker='token' if defending else 'witness';can_attack=defending or v!='fresh_attack' or 'Haste' in keys
            actions += [step('turn',player=1-owner if defending else owner,value=0),step('begin_attackers'),step('attack',ref=attacker,value=int(can_attack)),step('finish_attackers'),step('begin_blockers')]
            blocks=bool(witness and can_attack and (not defending and ('Flying' not in keys or v=='reach_blocker') or defending and (v!='block_flying' or bool(keys & {'Flying','Reach'}))))
            if witness and can_attack:actions += [step('block',ref=blocker,other=attacker,value=int(blocks))]
            actions += [step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
            if defending:entities['witness']['tapped']=True
            elif can_attack:token_tapped='Vigilance' not in keys
            if blocks:
                w=entities['witness'];deathtouch='Deathtouch' in keys
                if 'Lifelink' in keys:life[owner]+=t['power']
                lethal=1 if deathtouch else w['toughness'];dead_w=t['power']>=lethal;dead_token=w['power']>=t['toughness']
                if not defending and 'Trample' in keys:life[1-owner]-=max(0,t['power']-lethal)
                if dead_w:leave('witness','graveyard')
                else:w['damage']=t['power']
                if dead_token:count-=1;token_tapped=False
                else:token_damage=w['power']
            elif can_attack:
                life[owner if defending else 1-owner]-=entities['witness']['power'] if defending else t['power']
                if not defending and 'Lifelink' in keys:life[owner]+=t['power']
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,e in entities.items():
            checks.append(observe(ref,'in_play',int(e['alive'])))
            if e['alive']:
                checks.append(observe(ref,'tapped',int(e['tapped'])))
                if e['creature']:checks += [observe(ref,k,e[k]) for k in ('power','toughness','damage')]
        if count:
            properties=dict(token=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in t['colors']),artifact=int(t['artifact']),power=t['power'],toughness=t['toughness'])
            properties.update({'subtype_'+n:1 for n in t['subtypes']})
            for prop,value in properties.items():checks.append(step('assert_matching_permanents',player=owner,index=token_id,property=prop,value=value))
            for n in range(count):
                ref='expected_token_'+str(n);checks += [step('bind_permanent_copy',player=owner,value=token_id,index=n,ref=ref),observe(ref,'tapped',int(n==token_index and token_tapped)),observe(ref,'damage',token_damage if n==token_index else 0)]
        universe=set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}|{token_name}
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_zone',player=player,zone='battlefield',value=sum(e['alive'] and e['player']==player for e in entities.values())+(count if player==owner else 0)),step('assert_zone_multiplicity',player=player,zone='battlefield',index=token_id,value=count if player==owner else 0)]
            checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exiles[player]),('library',libs[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=n,value=rows[nm]['csvid']) for n,nm in enumerate(names)]
                else:
                    counts=Counter(names);checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
        steps=pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','effect_snapshot'):
            cut=0 if v=='ready_snapshot' else next(i for i,s in enumerate(actions) if s['op']=='cast')+1 if v=='pending_snapshot' else effect_cut
            tail=actions[cut:]+checks
            steps=pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if v=='resolved_snapshot':steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'TokenTriggerCreationRecipe_{csvid}_{requested}',card=name,fixture='empty_main' if bare else 'stocked_zones',seed=123,setup=setup,steps=steps,
          _recipe=dict(family='token_trigger_creation',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=['source passive keyword combat','separate trigger priority window','control changes','copy effects','token doublers','multiple simultaneous damage assignments']+p['gaps'])))
    return result


def expand_entry_component(name,csvid,d,p,selected,rows,o):
 if p.get('_followup_component'):return expand_followup_component(name,csvid,d,p,selected,rows,o)
 if p.get('_grant_component'):return expand_grant_component(name,csvid,d,p,selected,rows,o)
 from recipes import step,observe
 from token_creation_recipes import expand as entry_expand
 result=[]
 for requested in selected:
  mode,owner=requested.rsplit('_',1);owner=int(owner);counters=mode.startswith('fabricate_counters');mana_mode=mode.startswith('token_mana')
  underlying='create' if mana_mode else {'fabricate_counters':'create','fabricate_counters_cleanup':'cleanup','fabricate_counters_snapshot':'ready_snapshot'}.get(mode,mode)
  component=copy.deepcopy(p);component['_entry_component']=False;component['token']['name']=token_identity(p)
  if counters:component['token']['count']=0
  static=p.get('static_effect');living=2 if underlying=='two_casts' else 0 if underlying in ('source_destroyed','source_bounced','wrath','insufficient_mana','wrong_color','countered') else 1
  if static:
   component['token']['power']+=living*static['power'];component['token']['toughness']+=living*static['toughness']
   component['token']['keywords']+=static['keywords'] if living else []
  case=(expand if p['mode']=='death' else entry_expand)(name,csvid,d,component,[underlying+'_'+str(owner)],rows,o)[0]
  reserve=max(10,2*p['mana'][0]+5);extra=reserve-10 if underlying not in ('payment','insufficient_mana','wrong_color') else 0
  actions=[];pending=False;pending_ref='source';resolved=0;saved=None;tid=rows[token_identity(p)]['csvid'];live_refs={'source'} if underlying=='preloaded_source' else set()
  if extra:actions.append(step('set_mana',player=owner,color=0,value=reserve))
  for raw in case['steps']:
   action=copy.deepcopy(raw)
   if extra and action['op']=='assert_mana' and action['player']==owner and action['color']==0:action['value']+=extra
   if counters and action['op']=='assert_card' and action.get('ref') in ('source','second') and action.get('property') in ('power','toughness'):action['value']+=p['fabricate']
   if action['op']=='capture':saved=(pending,pending_ref,resolved,tuple(live_refs))
   if action['op']=='restore':pending,pending_ref,resolved,live_saved=saved;live_refs=set(live_saved)
   if name=='Maverick Thopterist' and mode=='bounce_recast' and action['op']=='cast' and action.get('ref')=='source' and action['value']==1 and resolved:
    # The earlier entry created eligible artifacts. Explicitly choose the
    # printed-mana route; Improvise payment remains a separate component gap.
    actions.append(step('choose_dialog',player=owner,value=1))
   if action['op']=='cast' and action.get('ref') in ('source','second','blink') and action['value']==1:pending=True;pending_ref=action['ref']
   if action['op']=='resolve' and pending and p['fabricate']:actions.append(step('choose_dialog',player=owner,value=0 if counters else 1))
   actions.append(action)
   if p.get('devoid_source') and action['op']=='assert_card' and action.get('ref') in ('source','second') and action.get('property')=='in_play' and action['value']==1:actions.append(step('assert_matching_permanents',player=owner,index=csvid,property='color',value=0))
   if action['op']=='resolve' and pending:
    pending=False;resolved+=1
    if p.get('devoid_source'):actions.append(step('assert_matching_permanents',player=owner,index=csvid,property='color',value=0))
    if p['mode']=='death':continue
    if pending_ref!='blink':live_refs.add(pending_ref)
    if counters:
     ref='source' if pending_ref=='blink' else pending_ref
     actions += [observe(ref,'power',p['power']+p['fabricate']),observe(ref,'toughness',p['toughness']+p['fabricate']),observe(ref,'plus1_counters',p['fabricate'])]
    else:
     power=p['token']['power']+(len(live_refs)*static['power'] if static else 0);toughness=p['token']['toughness']+(len(live_refs)*static['toughness'] if static else 0)
     actions += [step('assert_zone_multiplicity',player=owner,zone='battlefield',index=tid,value=resolved*p['token']['count']),step('assert_matching_permanents',player=owner,index=tid,property='power',value=power),step('assert_matching_permanents',player=owner,index=tid,property='toughness',value=toughness)]
     if p['fabricate']:actions += [observe('source' if pending_ref=='blink' else pending_ref,'plus1_counters',0)]
  case['steps']=actions;case['name']='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested
  gaps=['source passive keyword combat','separate entry-trigger priority window','control changes','copy effects','token doublers','multiple simultaneous damage assignments']+p['gaps']
  case['_recipe']=dict(family='token_trigger_creation',version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)
  if mana_mode:case=token_mana_case(case,p,mode,owner,rows)
  result.append(case)
 return result


def token_mana_case(case,p,mode,owner,rows):
 from recipes import step,observe
 tid=rows[token_identity(p)]['csvid'];count=p['token']['count'];drain=count if mode=='token_mana_all' else 1;remaining=count-drain
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0)
 prefix=copy.deepcopy(case['steps'][:cut]);checks=[]
 for raw in case['steps'][cut:]:
  action=copy.deepcopy(raw);ref=action.get('ref','')
  if action['op']=='assert_matching_permanents' and action['index']==tid and not remaining:continue
  if ref.startswith('expected_token_') and int(ref.rsplit('_',1)[1])>=remaining:continue
  if action['op']=='assert_zone' and action['player']==owner and action['zone']=='battlefield':action['value']-=drain
  if action['op']=='assert_zone_multiplicity' and action['player']==owner and action['zone']=='battlefield' and action['index']==tid:action['value']=remaining
  if action['op']=='assert_mana' and action['player']==owner and action['color']==0:action['value']+=drain
  checks.append(action)
 activation=[]
 for i in range(drain):
  activation += [step('bind_permanent_copy',player=owner,value=tid,index=0,ref='mana_token'),step('require_can_activate',ref='mana_token',value=1),step('activate',ref='mana_token',value=1),step('assert_stack',value=0),step('assert_choices_consumed'),observe('mana_token','in_play',0)]
 if mode=='token_mana_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(activation+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(activation+checks)+[step('assert_recorded_state')]
 else:case['steps']=prefix+activation+checks
 return case


def expand_grant_component(name,csvid,d,p,selected,rows,o):
 from recipes import step,observe
 result=[]
 for requested in selected:
  mode,owner=requested.rsplit('_',1);owner=int(owner);effect=p['static_effect'];keys=effect['keywords'];lost=mode=='grant_lost';scope=mode.startswith('haste_scope');subtype_scope=mode.startswith('grant_scope');owner_return=mode.startswith('owner_return');numeric_scope=mode.startswith('numeric_scope')
  underlying=({'Flying':'block_flying','First strike':'block_ground','Trample':'trample','Vigilance':'ready_attack','Haste':'fresh_attack'}[keys[0]] if lost else 'source_destroyed' if mode=='owner_return_source_lost' else 'create' if owner_return or numeric_scope else 'other_creature' if scope else 'create' if subtype_scope else mode)
  component=copy.deepcopy(p);component['_grant_component']=False
  if lost:component['static_effect']=None
  case=expand_entry_component(name,csvid,d,component,[underlying+'_'+str(owner)],rows,o)[0]
  living=2 if underlying=='two_casts' else 0 if underlying in ('source_destroyed','source_bounced','wrath','insufficient_mana','wrong_color','countered') else 1
  setup={c['ref']:c for c in case['setup']};actions=[];live_refs={'source'} if underlying=='preloaded_source' else set();pending=None;saved=None;deferred_proofs=[]
  for raw in case['steps']:
   action=copy.deepcopy(raw)
   if not lost and action['op']=='assert_card' and action.get('property') in ('power','toughness'):
    ref=action.get('ref');item=setup.get(ref);eligible=bool(item and item['player']==owner and 'Creature' in o[item['name']]['type_line']);amount=0
    if eligible and p['grant_scope']=='all_other':amount=max(0,living-1) if ref in ('source','second') else living
    if eligible and p['grant_scope']=='soldier_other' and 'Soldier' in o[item['name']]['type_line']:amount=max(0,living-1) if ref in ('source','second') else living
    action['value']+=amount*effect[action['property']]
   if not lost and 'First strike' in keys and underlying=='block_ground' and action['op']=='assert_card' and action.get('ref')=='expected_token_0' and action.get('property')=='damage':action['value']=0
   if action['op']=='capture':saved=(tuple(live_refs),pending,copy.deepcopy(deferred_proofs))
   if action['op']=='restore':saved_refs,pending,deferred_saved=saved;live_refs=set(saved_refs);deferred_proofs=copy.deepcopy(deferred_saved)
   if action['op']=='cast' and action.get('ref') in ('source','second','blink') and action['value']==1:pending=action['ref']
   if 'Lifelink' in p['token']['keywords'] and underlying in ('ready_attack','block_ground') and action['op']=='assert_life' and action['player']==owner:action['value']+=p['token']['power']+effect['power']
   actions.append(action)
   if action['op']=='bind_permanent' and action.get('ref')=='source' and deferred_proofs:actions+=deferred_proofs;deferred_proofs=[]
   if action['op']=='resolve' and pending:
    if pending!='blink':live_refs.add(pending)
    if not lost and p['grant_scope']=='all_other':
     actions += [observe('unselected',prop,2+len(live_refs)*effect[prop]) for prop in ('power','toughness')]
     for ref in sorted(live_refs):
      proofs=[observe(ref,prop,p[prop]+(p['fabricate'] if mode.startswith('fabricate_counters') else 0)+max(0,len(live_refs)-1)*effect[prop]) for prop in ('power','toughness')]
      if pending=='blink' and ref=='source':deferred_proofs+=proofs
      else:actions+=proofs
    pending=None
  case['steps']=actions
  if lost:
   cut=next(i for i,s in enumerate(actions) if s['op']=='begin_attackers');actions[cut:cut]=[step('destroy',ref='source'),step('refresh')]
   altered=[]
   for i,raw in enumerate(actions):
    a=copy.deepcopy(raw)
    if i<cut and a['op']=='assert_matching_permanents' and a['index']==rows[token_identity(p)]['csvid'] and a.get('property') in ('power','toughness'):a['value']+=effect[a['property']]
    if a['op']=='assert_card' and a.get('ref')=='source':
     if a['property']!='in_play':continue
     a['value']=0
    if a['op']=='assert_zone' and a['player']==owner:
     if a['zone']=='battlefield':a['value']-=1
     if a['zone']=='graveyard':a['value']+=1
    if a['op']=='assert_zone_multiplicity' and a['player']==owner and a['zone']=='graveyard' and a['index']==csvid:a['value']+=1
    altered.append(a)
   case['steps']=altered
  if owner_return:case=owner_return_case(case,p,mode,owner,rows,o)
  if numeric_scope:case=numeric_grant_scope_case(case,p,mode,owner,rows,o)
  if scope:case=haste_scope_case(case,p,mode,owner,rows,o)
  if subtype_scope:case=grant_subtype_scope_case(case,p,mode,owner,rows,o)
  case['name']='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested;case['_recipe']['variant']=requested
  case['_recipe']['untested_components']=[('multi-blocker damage assignment choices' if gap=='multiple simultaneous damage assignments' else gap) for gap in case['_recipe']['untested_components']]
  case['_recipe']['untested_components'] += ['source attack/block keyword behavior; creature keyword grants are tested on created tokens','opposing Golem recipients and live type/control changes' if p['grant_scope']=='golem' else 'live type/control changes beyond the declared independent controls']
  result.append(case)
 return result


def haste_scope_case(case,p,mode,owner,rows,o):
 from recipes import card,step,observe
 source_id=rows[p['source_name']]['csvid'];tid=rows[token_identity(p)]['csvid'];orn=rows['Myr Retriever']['csvid']
 case['setup'] += [card('Myr Retriever','own_artifact',owner,'hand'),card('Myr Retriever','opponent_artifact',1-owner,'hand')]
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[]
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_zone' and a['zone']=='battlefield':a['value']+=1
  if a['op']=='assert_mana' and a['color']==0:a['value']-=2
  if a['op']=='assert_life' and a['player']==1-owner:a['value']-=p['token']['power']+1
  if a['op']=='assert_card' and a.get('ref')=='expected_token_0' and a['property']=='tapped':a['value']=1
  checks.append(a)
 for ref,player,tapped in [('own_artifact',owner,1),('opponent_artifact',1-owner,0)]:
  checks += [observe(ref,'in_play',1),observe(ref,'power',1),observe(ref,'toughness',1),observe(ref,'damage',0),observe(ref,'tapped',tapped),step('assert_zone_multiplicity',player=player,zone='battlefield',index=orn,value=1),step('assert_matching_permanents',player=player,index=orn,property='artifact',value=1),step('assert_matching_permanents',player=player,index=orn,property='token',value=0)]
  for zone in ('hand','graveyard','exile','library'):checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=orn,value=0)]
 run=[step('cast',ref='own_artifact',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('bind_permanent_copy',player=owner,value=tid,index=0,ref='scope_token'),step('begin_attackers'),step('attack',ref='scope_token',value=1),step('attack',ref='own_artifact',value=1),step('attack',ref='other',value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat'),step('turn',player=1-owner,value=0),step('cast',ref='opponent_artifact',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('begin_attackers'),step('attack',ref='opponent_artifact',value=0),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('resolve'),step('end_combat')]+checks
 if mode=='haste_scope_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 return case



def grant_subtype_scope_case(case,p,mode,owner,rows,o):
 from recipes import card,step,observe
 keyword=p['static_effect']['keywords'][0];tid=rows[token_identity(p)]['csvid'];bear=rows['Grizzly Bears']['csvid'];hawk=rows['Suntail Hawk']['csvid']
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[]
 deaths=[0,0];new_bf=[0,0];mana=[];life=[20,20];token_damage=0;token_tapped=False;bear_damage=0;bear_tapped=False;bear_dead=False
 if keyword=='First strike':
  case['setup'] += [card('Grizzly Bears','positive_witness',1-owner,'battlefield'),card('Grizzly Bears','negative_witness',1-owner,'battlefield')]
  bear_dead=True;deaths[owner]=1;deaths[1-owner]=2;new_bf[owner]=-1
  run=[step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='positive_witness',value=1),step('attack',ref='negative_witness',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='scope_token',other='positive_witness',value=1),step('block',ref='unselected',other='negative_witness',value=1)]
 elif keyword=='Flying':
  case['setup'] += [card('Suntail Hawk','positive_witness',1-owner,'battlefield'),card('Suntail Hawk','negative_witness',1-owner,'battlefield')]
  deaths[1-owner]=1;new_bf[1-owner]=1;life[owner]-=1;token_damage=1
  run=[step('turn',player=1-owner,value=0),step('begin_attackers'),step('attack',ref='positive_witness',value=1),step('attack',ref='negative_witness',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='scope_token',other='positive_witness',value=1),step('block',ref='unselected',other='negative_witness',value=0)]
 elif keyword=='Vigilance':
  bear_tapped=True;life[1-owner]-=p['token']['power']+2
  run=[step('advance_turn'),step('advance_turn'),step('turn',player=owner,value=0),step('begin_attackers'),step('attack',ref='scope_token',value=1),step('attack',ref='unselected',value=1),step('finish_attackers'),step('begin_blockers')]
 elif keyword=='Trample':
  case['setup'] += [card('Grizzly Bears','positive_witness',1-owner,'battlefield'),card('Suntail Hawk','negative_witness',1-owner,'battlefield')]
  deaths[1-owner]=2;life[1-owner]-=max(0,p['token']['power']-2);token_damage=2;token_tapped=True;bear_damage=1;bear_tapped=True
  run=[step('advance_turn'),step('advance_turn'),step('turn',player=owner,value=0),step('begin_attackers'),step('attack',ref='scope_token',value=1),step('attack',ref='unselected',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='positive_witness',other='scope_token',value=1),step('block',ref='negative_witness',other='unselected',value=1)]
 else:raise ValueError('reviewed Golem grant scope required')
 run=[step('bind_permanent_copy',player=owner,value=tid,index=0,ref='scope_token')]+run+[step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_life':a['value']=life[a['player']]
  if a['op']=='assert_zone':
   if a['zone']=='battlefield':a['value']+=new_bf[a['player']]
   if a['zone']=='graveyard':a['value']+=deaths[a['player']]
  if a['op']=='assert_zone_multiplicity' and a['zone']=='graveyard' and a['index']==bear:
   a['value']+=(1 if a['player']==owner and bear_dead else 2 if a['player']==1-owner and keyword=='First strike' else 1 if a['player']==1-owner and keyword=='Trample' else 0)
  if a['op']=='assert_card' and a.get('ref')=='unselected':
   if bear_dead:
    if a['property']!='in_play':continue
    a['value']=0
   if a['property']=='damage':a['value']=bear_damage
   if a['property']=='tapped':a['value']=int(bear_tapped)
  if a['op']=='assert_card' and a.get('ref')=='expected_token_0':
   if a['property']=='damage':a['value']=token_damage
   if a['property']=='tapped':a['value']=int(token_tapped)
  checks.append(a)
 if keyword!='Vigilance':
  checks += [observe('positive_witness','in_play',0),observe('negative_witness','in_play',int(keyword=='Flying'))]
  if keyword=='Flying':checks += [observe('negative_witness','power',1),observe('negative_witness','toughness',1),observe('negative_witness','damage',0),observe('negative_witness','tapped',1)]
 if keyword in ('Flying','Trample'):
  for player in (0,1):
   for zone in ('hand','battlefield','graveyard','exile','library'):
    count=(1 if zone=='graveyard' and player==1-owner else 1 if zone=='battlefield' and player==1-owner and keyword=='Flying' else 0)
    checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=hawk,value=count))
 run+=checks
 if mode=='grant_scope_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 return case


def expand_event_component(name,csvid,d,p,selected,rows,o):
 from recipes import step,cost_from_oracle
 from token_trigger_creation_spec import entry_profiles
 profile=entry_profiles()[name];result=[]
 for requested in selected:
  mode,owner=requested.rsplit('_',1);owner=int(owner);mana_mode=mode.startswith('token_mana');underlying='create' if mana_mode else mode
  component=dict(p,_event_component=False);case=expand(name,csvid,d,component,[underlying+'_'+str(owner)],rows,o)[0]
  reserve=max(10,2*p['mana'][0]+2*(p['activation']['mana'][0] if p['activation'] else 0)+5);extra=reserve-10 if underlying not in ('payment','insufficient_mana','wrong_color') else 0
  pool=[10]*6;pool[0]+=extra;actions=[step('set_mana',player=owner,color=0,value=reserve)] if extra else [];saved=None
  for raw in case['steps']:
   a=copy.deepcopy(raw)
   if a['op']=='capture':saved=list(pool)
   if a['op']=='restore':pool=list(saved)
   if a['op']=='set_mana' and a['player']==owner:pool[a['color']]=a['value']
   if a['op']=='cast' and a['value']==1 and a.get('ref') in ('source','second'):pool=[x-y for x,y in zip(pool,p['mana'])]
   if a['op']=='activate' and a.get('ref')=='source' and a['value']==1:
    menu=profile.get('activation_menu');show=menu is not None
    if profile.get('menu_other_cost'):show=all(x>=y for x,y in zip(pool,profile['menu_other_cost']))
    if show:actions.append(step('choose_dialog',player=owner,value=menu))
    pool=[x-y for x,y in zip(pool,p['activation']['mana'])]
   if extra and a['op']=='assert_mana' and a['player']==owner and a['color']==0 and underlying not in ('activation_payment','activation_insufficient'):a['value']+=extra
   if name=='Spawnsire of Ulamog' and underlying in ('block_ground','block_flying') and a['op']=='finish_attackers':
    actions += [step('choose_target',player=1-owner,ref='witness'),a,step('assert_unused_choices',value=1)]
   else:actions.append(a)
  case['steps']=actions;case['name']='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested;case['_recipe']['variant']=requested
  case['_recipe']['untested_components']+=['unselected activated ability modes; card-wide activation legality is not a selected-mode rejection probe'] if p['mode']=='activation' else []
  if p.get('_landfall_functional_priority'):
   case['_recipe']['untested_components']+=['mandatory trigger enforcement outside the dedicated no-acceptance human create_0 control; sibling functional variants explicitly accept the legacy priority window']
  if mana_mode:case=token_mana_case(case,p,mode,owner,rows)
  result.append(case)
 return result


def expand_followup_component(name,csvid,d,p,selected,rows,o):
 from recipes import step
 result=[]
 special={'own_artifact','power_artifact','power_artifact_snapshot','power_wrath','power_wrath_snapshot','opponent_artifact','source_lost_artifact','nonartifact_death','hexproof_opponent'}
 for requested in selected:
  mode,owner=requested.rsplit('_',1);owner=int(owner);underlying='fabricate_counters' if mode.startswith(('power_artifact','power_wrath')) else 'source_destroyed' if mode=='source_lost_artifact' else 'create' if mode in special else mode
  component=dict(p,_followup_component=False);case=expand_entry_component(name,csvid,d,component,[underlying+'_'+str(owner)],rows,o)[0]
  if mode.startswith('power_wrath'):case=artifact_power_wrath_control(case,p,mode,owner,rows,o)
  elif mode in special:case=artifact_loss_control(case,p,mode,owner,rows,o)
  else:
   loss=p['power']*(p['token']['count'] if mode=='wrath' else 1 if mode in ('destroy_token','block_ground') else 0)
   for a in case['steps']:
    if a['op']=='assert_life' and a['player']==1-owner:a['value']-=loss
  case['name']='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested;case['_recipe']['variant']=requested
  case['_recipe']['untested_components']+=['multiple independent opponents and target redirection','artifact creature type changes','power-changing continuous effects beyond Fabricate counters']
  result.append(case)
 return result


def artifact_loss_control(case,p,mode,owner,rows,o):
 from recipes import card,step,observe
 tid=rows[token_identity(p)]['csvid'];bone=rows['Bone Saw']['csvid'];orb=rows['Orbs of Warding']['csvid'];bear=rows['Grizzly Bears']['csvid'];hexproof=mode=='hexproof_opponent';nonartifact=mode=='nonartifact_death';caster=1-owner if mode in ('opponent_artifact','hexproof_opponent') else owner;nm='Orbs of Warding' if hexproof else 'Bone Saw';ref='warding' if hexproof else 'artifact_witness'
 if not nonartifact:case['setup'].append(card(nm,ref,caster,'hand'))
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[];power=p['power']+(p['fabricate'] if mode.startswith('power_artifact') else 0);loss=power if mode in ('own_artifact','power_artifact','power_artifact_snapshot') else 0
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_life' and a['player']==1-owner:a['value']-=loss
  if a['op']=='assert_zone':
   if a['zone']=='graveyard' and a['player']==(owner if nonartifact else caster) and not hexproof:a['value']+=1
   if a['zone']=='battlefield':
    if a['player']==owner and (nonartifact or hexproof):a['value']-=1
    if a['player']==caster and hexproof:a['value']+=1
  if a['op']=='assert_mana' and a['player']==caster and a['color']==0 and hexproof:a['value']-=5
  if a['op']=='assert_zone_multiplicity' and a['zone']=='graveyard' and a['player']==owner and a['index']==bear and nonartifact:a['value']+=1
  if a['op']=='assert_zone_multiplicity' and a['zone']=='battlefield' and a['player']==owner and a['index']==tid and hexproof:a['value']-=1
  if a['op']=='assert_card' and a.get('ref')=='unselected' and nonartifact:
   if a['property']!='in_play':continue
   a['value']=0
  if hexproof and a.get('ref')=='expected_token_2':continue
  checks.append(a)
 if nonartifact:run=[step('destroy',ref='unselected'),step('refresh')]
 else:
  run=[step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
  if hexproof:run += [step('bind_permanent_copy',player=owner,value=tid,index=0,ref='loss_token'),step('destroy',ref='loss_token'),step('refresh'),observe(ref,'in_play',1)]
  else:run += [step('destroy',ref=ref),step('refresh'),observe(ref,'in_play',0)]
  for player in (0,1):
   for zone in ('hand','battlefield','graveyard','exile','library'):
    checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=orb if hexproof else bone,value=int(player==caster and zone==('battlefield' if hexproof else 'graveyard'))))
 run+=checks
 if mode=='power_artifact_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 return case



def artifact_power_wrath_control(case,p,mode,owner,rows,o):
 from recipes import card,step,observe
 bone=rows['Cathodion']['csvid'];wrath=rows['Wrath of God']['csvid'];bear=rows['Grizzly Bears']['csvid'];source=rows[p['source_name']]['csvid'];power=p['power']+p['fabricate'];case['setup'] += [card('Cathodion','artifact_witness',owner,'hand'),card('Wrath of God','power_wrath',0,'hand')]
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[]
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_life' and a['player']==1-owner:a['value']-=power
  if a['op']=='assert_zone':
   if a['zone']=='battlefield' and a['player']==owner:a['value']-=2
   if a['zone']=='graveyard':a['value']+=(3 if a['player']==owner else 0)+(1 if a['player']==0 else 0)
  if a['op']=='assert_zone_multiplicity' and a['zone']=='graveyard':
   if a['player']==owner and a['index'] in (source,bear):a['value']+=1
   if a['player']==0 and a['index']==wrath:a['value']+=1
  if a['op']=='assert_mana' and a['player']==0 and a['color'] in (0,5):a['value']-=2
  if a['op']=='assert_card' and a.get('ref') in ('source','unselected'):
   if a['property']!='in_play':continue
   a['value']=0
  checks.append(a)
 for player in (0,1):
  for zone in ('hand','battlefield','graveyard','exile','library'):checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=bone,value=int(player==owner and zone=='graveyard')))
 run=[step('cast',ref='artifact_witness',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),observe('source','power',power),observe('artifact_witness','in_play',1),observe('artifact_witness','power',3),observe('artifact_witness','toughness',3),step('assert_matching_permanents',player=owner,index=bone,property='artifact',value=1),step('assert_mana',player=owner,color=0,value=max(10,2*p['mana'][0]+5)-p['mana'][0]-3),step('cast',ref='power_wrath',value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),observe('artifact_witness','in_play',0)]+checks
 if mode=='power_wrath_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 return case


def owner_return_case(case,p,mode,owner,rows,o):
 from recipes import card,step,observe
 for c in case['setup']:
  if c['ref']=='unselected':c['state']=['opponent_owned']
 other=card('Ashcoat Bear','lost_owned',1-owner,'battlefield');other['state']=['opponent_owned'];case['setup'].append(other)
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[];returns=mode in ('owner_return','owner_return_snapshot');source_gone=mode=='owner_return_source_lost'
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_zone' and a['player']==1-owner and a['zone']=='battlefield':a['value']+=1
  if a['op']=='assert_card' and a.get('ref')=='unselected' and a.get('property') in ('power','toughness') and returns:a['value']=2
  checks.append(a)
 checks += [observe('lost_owned','in_play',1),observe('lost_owned','power',3 if returns else 2),observe('lost_owned','toughness',3 if returns else 2),observe('lost_owned','damage',0),observe('lost_owned','tapped',0)]
 for nm,controller in [('Grizzly Bears',1-owner if returns else owner),('Ashcoat Bear',owner if returns else 1-owner)]:
  for player in (0,1):
   checks.append(step('assert_zone_multiplicity',player=player,zone='battlefield',index=rows[nm]['csvid'],value=int(player==controller)))
   if nm=='Ashcoat Bear':
    for zone in ('hand','graveyard','exile','library'):checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=rows[nm]['csvid'],value=0))
 run=[observe('unselected','power',2 if source_gone else 3),observe('lost_owned','power',2),step('turn',player=1-owner if mode=='owner_return_wrong_turn' else owner,value=0),step('cleanup'),step('refresh')]
 if returns:run += [step('bind_permanent',player=1-owner,value=rows['Grizzly Bears']['csvid'],ref='unselected'),step('bind_permanent',player=owner,value=rows['Ashcoat Bear']['csvid'],ref='lost_owned')]
 run+=checks
 if mode=='owner_return_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 case['_recipe']['untested_components']=[('control changes beyond the end-step owner-return component' if gap=='control changes' else gap) for gap in case['_recipe']['untested_components']]
 return case


def numeric_grant_scope_case(case,p,mode,owner,rows,o):
 from recipes import card,step,observe,cost_from_oracle
 from token_trigger_creation_spec import component_creation
 nm='Chatter of the Squirrel' if p['grant_scope']=='squirrel_all' else 'Raise the Alarm';spell=o[nm];body=spell['oracle_text'].splitlines()[0];token,mana=component_creation(body);assert not mana;caster=1-owner;tid=rows[token_identity(p)]['csvid'];source=rows[p['source_name']]['csvid'];spell_id=rows[nm]['csvid'];bonus=1 if p['grant_scope']=='squirrel_all' else 0;case['setup'].append(card(nm,'scope_spell',caster,'hand'))
 cut=next(i for i,s in enumerate(case['steps']) if s['op']=='assert_stack' and s['value']==0);prefix=copy.deepcopy(case['steps'][:cut]);checks=[]
 for raw in case['steps'][cut:]:
  a=copy.deepcopy(raw)
  if a['op']=='assert_card' and a.get('ref')=='source':
   if a['property']!='in_play':continue
   a['value']=0
  if a['op']=='assert_matching_permanents' and a['player']==owner and a['index']==tid and a['property'] in ('power','toughness'):a['value']-=1
  if a['op']=='assert_zone':
   if a['zone']=='battlefield':a['value']+=-1 if a['player']==owner else token['count']
   if a['zone']=='graveyard':a['value']+=1
  if a['op']=='assert_zone_multiplicity':
   if a['player']==owner and a['zone']=='graveyard' and a['index']==source:a['value']+=1
   if a['player']==caster and a['zone']=='battlefield' and a['index']==tid:a['value']+=token['count']
  if a['op']=='assert_mana' and a['player']==caster:a['value']-=cost_from_oracle(spell)[a['color']]
  checks.append(a)
 for player in (0,1):
  for zone in ('hand','battlefield','graveyard','exile','library'):checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=spell_id,value=int(player==caster and zone=='graveyard')))
 props=dict(token=1,power=1,toughness=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in token['colors']),artifact=int(token['artifact']));props.update({'subtype_'+n:1 for n in token['subtypes']})
 checks += [step('assert_matching_permanents',player=caster,index=tid,property=k,value=v) for k,v in props.items()]
 run=[step('turn',player=caster,value=0),step('cast',ref='scope_spell',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),step('assert_zone_multiplicity',player=caster,zone='battlefield',index=tid,value=token['count'])]+[step('assert_matching_permanents',player=caster,index=tid,property=prop,value=1+bonus) for prop in ('power','toughness')]+[step('destroy',ref='source'),step('refresh')]+checks
 if mode=='numeric_scope_snapshot':case['steps']=prefix+[step('capture')]+copy.deepcopy(run)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(run)+[step('assert_recorded_state')]
 else:case['steps']=prefix+run
 return case



# Reusable variable-count ledger. Normal cards and actual token events are
# modeled from Oracle; registered token rows supply identity only.
VARIABLE_ZONES=('hand','battlefield','graveyard','exile','library')
from token_trigger_creation_spec import variable_data,variable_cost,variable_variants,validate_variable_witnesses,VARIABLE_COLOR_INDEX

def _variable_step(op,**kw):return dict(op=op,**kw)
def _variable_observe(ref,prop,value):return _variable_step('assert_card',ref=ref,property=prop,value=value)

class VariableTokenLedger:
 """Scenario state derived from explicit cards and actual scripted events."""
 def __init__(self,owner,oracle,rows,data=None):
  self.data=data or variable_data();self.owner=owner;self.oracle=oracle;self.rows=rows;self.setup=[];self.entities={};self.zones={z:[[],[]] for z in VARIABLE_ZONES};self.tokens=[];self.mana=[[30,10,10,10,10,10] for _ in (0,1)];self.life=[20,20];self.turn=owner;self.serial=0;self.deaths=[];self.steps=[];self.universe=set();self.pending_cut=None
 def emit(self,*actions):self.steps.extend(actions)
 def add(self,name,ref,player=None,zone='hand',opponent_owned=False,tapped=False):
  player=self.owner if player is None else player;owner=1-player if opponent_owned else player;record=self.oracle[name];item=dict(name=name,ref=ref,player=player,zone=zone);state=[]
  if opponent_owned:state.append('opponent_owned')
  if tapped:state.append('tapped')
  if state:item['state']=state
  self.setup.append(item);self.universe.add(name);self.zones[zone][player].append(name);self.entities[ref]=dict(name=name,player=player,owner=owner,zone=zone,initial_zone=zone,creature='Creature' in record['type_line'],power=int(record.get('power') or 0),toughness=int(record.get('toughness') or 0),tapped=tapped,damage=0)
  return ref
 def pay(self,player,c):self.mana[player]=[a-b for a,b in zip(self.mana[player],c)];assert min(self.mana[player])>=0
 def pool(self,player,c):
  self.mana[player]=list(c);self.emit(*(_variable_step('set_mana',player=player,color=i,value=v) for i,v in enumerate(c)))
 def move(self,ref,zone):
  e=self.entities[ref];old=e['zone'];self.zones[old][e['player']].remove(e['name']);dest=e['player'] if zone=='battlefield' else e['owner'];e['player']=dest;e['zone']=zone;self.zones[zone][dest].append(e['name'])
  if old=='battlefield' and zone=='graveyard' and e['creature']:self.deaths.append(dict(owner=dest,name=e['name'],token=False,serial=self.serial))
 def token_def(self,name):
  return next(t for t in self.tokens if t['name']==name)
 def create(self,definition,amount,player=None,origin='source'):
  player=self.owner if player is None else player;self.universe.add(definition['name'])
  for _ in range(amount):self.tokens.append(dict(copy.deepcopy(definition),player=player,owner=player,origin=origin,counters=0,tapped=False,damage=0,alive=True))
 def count(self,name,player):return sum(t['alive'] and t['player']==player and t['name']==name for t in self.tokens)
 def quantity(self,p):
  if p['basis']=='controlled_lands':return sum(e['zone']=='battlefield' and e['player']==self.owner and 'Land' in self.oracle[e['name']]['type_line'] for e in self.entities.values())
  if p['basis']=='devotion_white':return sum(self.oracle[e['name']]['mana_cost'].count('{W}') for e in self.entities.values() if e['zone']=='battlefield' and e['player']==self.owner)
  if p['basis']=='creature_cards_in_own_graveyard':return sum('Creature' in self.oracle[n]['type_line'] for n in self.zones['graveyard'][self.owner])
  return sum(d['owner']==self.owner and not d['token'] and d['serial']==self.serial for d in self.deaths)
 def source_event(self,p):self.create(p['token'],self.quantity(p))
 def proof_tokens(self):
  for player in (0,1):
   for name in sorted({t['name'] for t in self.tokens}):
    live=[t for t in self.tokens if t['alive'] and t['name']==name and t['player']==player];tid=self.rows[name]['csvid'];self.emit(_variable_step('assert_zone_multiplicity',player=player,zone='battlefield',index=tid,value=len(live)))
    if not live:continue
    for prop,value in [('token',1),('artifact',int(live[0]['artifact'])),('color',sum(1<<VARIABLE_COLOR_INDEX[c] for c in live[0]['colors']))]:self.emit(_variable_step('assert_matching_permanents',player=player,index=tid,property=prop,value=value))
    for subtype in live[0]['subtypes']:self.emit(_variable_step('assert_matching_permanents',player=player,index=tid,property='subtype_'+subtype,value=1))
    # Individual binds keep counter/body/tap/damage receipts meaningful if later
    # expansion introduces more than one counter state for a shared identity.
    for i,t in enumerate(live):
     ref='receipt_'+str(player)+'_'+name.replace(' ','_')+'_'+str(i);self.emit(_variable_step('bind_permanent_copy',player=player,value=tid,index=i,ref=ref))
     for prop,value in [('power',t['power']+t['counters']),('toughness',t['toughness']+t['counters']),('plus1_counters',t['counters']),('tapped',int(t['tapped'])),('damage',t['damage'])]:self.emit(_variable_observe(ref,prop,value))
 def cast_source(self,p,ref='source'):
  e=self.entities[ref];before=self.zones['hand'][e['player']].count(e['name']);self.emit(_variable_step('cast',ref=ref,value=1));self.pending_cut=len(self.steps);self.pay(e['player'],variable_cost(self.oracle[e['name']]))
  self.emit(_variable_step('assert_stack',value=1),_variable_step('assert_zone_multiplicity',player=e['player'],zone='hand',index=self.rows[e['name']]['csvid'],value=before))
  self.emit(*(_variable_step('assert_mana',player=e['player'],color=i,value=v) for i,v in enumerate(self.mana[e['player']])))
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.move(ref,'battlefield')
  if p['event']=='entry':self.source_event(p)
 def spell(self,name,ref,target=None,player=None):
  player=self.owner if player is None else player;self.add(name,ref,player)
  if target:self.emit(_variable_step('choose_target',player=player,ref=target))
  self.emit(_variable_step('cast',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(player,variable_cost(self.oracle[name]));self.move(ref,'graveyard')
 def destroy(self,ref):self.emit(_variable_step('destroy',ref=ref),_variable_step('refresh'));self.move(ref,'graveyard')
 def drop_token(self,token,dies=True):
  token['alive']=False
  if dies:self.deaths.append(dict(owner=token['owner'],token=True,name=token['name'],serial=self.serial))
 def kill_tokens(self):
  for t in self.tokens:
   if t['alive']:self.drop_token(t)
 def wrath(self,p,source_dies=False):
  self.spell('Wrath of God','wrath')
  for ref,e in self.entities.items():
   if e['zone']=='battlefield' and e['creature']:self.move(ref,'graveyard')
  self.kill_tokens()
  if source_dies:self.source_event(p)
 def purge(self):
  self.add("Tormod's Crypt",'crypt');self.emit(_variable_step('cast',ref='crypt',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('assert_choices_consumed'));self.move('crypt','battlefield')
  self.pay(self.owner,variable_cost(self.oracle["Tormod's Crypt"]))
  self.emit(_variable_step('choose_player',player=self.owner,value=self.owner),_variable_step('require_can_activate',ref='crypt',value=1),_variable_step('activate',ref='crypt',value=1),_variable_step('assert_stack',value=1));self.move('crypt','graveyard');self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'))
  moved=list(self.zones['graveyard'][self.owner]);self.zones['exile'][self.owner]+=moved;self.zones['graveyard'][self.owner]=[]
  for e in self.entities.values():
   if e['zone']=='graveyard' and e['player']==self.owner:e['zone']='exile'
 def atlas(self,ref,land,player=None,accept=True,p=None):
  player=self.owner if player is None else player;self.emit(_variable_step('require_can_activate',ref=ref,value=1),_variable_step('activate',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('choose_zone_card',player=player,value=self.rows[self.entities[land]['name']]['csvid']))
  source_alive='source' in self.entities and self.entities['source']['zone']=='battlefield'
  triggers=bool(p and p['mandatory_followups'] and source_alive and player==self.owner)
  if triggers and accept and self.owner==0:self.emit(_variable_step('choose_trigger',player=0,ref='source'))
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.entities[ref]['tapped']=True;self.move(land,'battlefield');self.entities[land]['tapped']=self.entities[land]['name']=='Khalni Garden';self.emit(_variable_observe(land,'in_play',1),_variable_observe(land,'tapped',int(self.entities[land]['tapped'])))
  if self.entities[land]['name']=='Khalni Garden':
   component=self.data['witness_token_components']['Khalni Garden'];self.create(component['token'],component['count'],player,'garden')
  if triggers and accept:
   for t in self.tokens:
    if t['alive'] and t['player']==self.owner and 'Plant' in t['subtypes']:t['counters']+=1
 def finish(self):
  self.emit(_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'));self.proof_tokens()
  for ref,e in self.entities.items():
   # Initial library/grave/exile setup refs are zone offsets, not live
   # card-instance handles. Their full receipts are the zone checks below.
   if e['initial_zone'] not in ('hand','battlefield'):continue
   alive=e['zone']=='battlefield';self.emit(_variable_observe(ref,'in_play',int(alive)))
   if alive:
    self.emit(_variable_observe(ref,'tapped',int(e['tapped'])))
    if e['creature']:
     for prop in ('power','toughness','damage'):self.emit(_variable_observe(ref,prop,e[prop]))
     self.emit(_variable_observe(ref,'plus1_counters',0))
  for player in (0,1):
   self.emit(_variable_step('assert_life',player=player,value=self.life[player]));self.emit(*(_variable_step('assert_mana',player=player,color=i,value=v) for i,v in enumerate(self.mana[player])))
   for zone in VARIABLE_ZONES:
    names=list(self.zones[zone][player])
    if zone=='battlefield':names += [t['name'] for t in self.tokens if t['alive'] and t['player']==player]
    self.emit(_variable_step('assert_zone',player=player,zone=zone,value=len(names)))
    counts=Counter(names)
    for name in sorted(self.universe):self.emit(_variable_step('assert_zone_multiplicity',player=player,zone=zone,index=self.rows[name]['csvid'],value=counts[name]))
    if zone=='library':self.emit(*(_variable_step('assert_zone_card',player=player,zone=zone,index=i,value=self.rows[name]['csvid']) for i,name in enumerate(names)))


def prepare_variable_inputs(b,p,mode,data):
 """Prepare count inputs using fixture state or actual printed witness actions."""
 owner=b.owner;basis=p['basis'];minimum=mode in ('minimum','insufficient_mana','wrong_color');n=0 if minimum else 4 if mode=='larger_amount' else 2;opposite=mode=='opponent_state';side=1-owner if opposite else owner
 if basis=='controlled_lands':
  b.add('Walking Atlas','atlas',owner,'battlefield');b.add('Forest','next_land',owner)
  if mode.startswith('landfall_') or mode=='atlas_tapped_denied':
   b.add('Walking Atlas','opponent_atlas',1-owner,'battlefield');b.add('Khalni Garden','own_garden',owner);b.add('Khalni Garden','opponent_garden',1-owner);b.add('Walking Atlas','garden_atlas',owner,'battlefield');b.add('Grizzly Bears','ordinary_control',owner,'battlefield')
   b.add('Forest','base_land',owner,'battlefield',tapped=True);b.atlas('garden_atlas','own_garden');b.atlas('opponent_atlas','opponent_garden',1-owner);b.add('Walking Atlas','second_atlas',owner,'battlefield');b.add('Plains','second_land',owner)
  elif mode=='ownership':
   b.add('Forest','foreign_land',owner,'battlefield',opponent_owned=True,tapped=True);b.add('Plains','owned_opposing_land',1-owner,'battlefield',opponent_owned=True,tapped=True)
  else:
   for i in range(n):b.add('Forest','counting_land_'+str(i),side,'battlefield',tapped=True)
 elif basis=='devotion_white':
  if mode=='tokens_do_not_contribute':n=0
  if mode=='ownership':
   b.add('Suntail Hawk','foreign_white',owner,'battlefield',opponent_owned=True);b.add('Benevolent Bodyguard','owned_opposing_white',1-owner,'battlefield',opponent_owned=True)
  else:
   for i in range(n):b.add('Suntail Hawk','white_'+str(i),side,'battlefield')
 elif basis=='own_grave_nontoken_deaths_this_turn':
  if mode=='tokens_do_not_contribute':n=0
  if mode=='death_via_spell':n=1
  if mode=='ownership':
   b.add('Grizzly Bears','foreign_creature',owner,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing_creature',1-owner,'battlefield',opponent_owned=True);b.destroy('foreign_creature');b.destroy('owned_opposing_creature')
  else:
   for i in range(n):
    ref='fodder_'+str(i);b.add('Grizzly Bears',ref,side,'battlefield')
    if mode=='death_via_spell':b.spell('Murder','murder',ref);b.move(ref,'graveyard')
    else:b.destroy(ref)
  if mode=='prior_turn_expired':
   # Initialization reset occurred before deaths. Expiry itself uses the real
   # start-turn gate, never a second reset_death_history operation.
   b.emit(_variable_step('advance_turn'));b.turn=1-b.turn;b.serial+=1
  if mode=='purged_history':b.purge()
 else:
  if mode=='tokens_do_not_contribute':n=0
  if mode=='ownership':
   b.add('Grizzly Bears','foreign_creature',owner,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing_creature',1-owner,'battlefield',opponent_owned=True);b.destroy('foreign_creature');b.destroy('owned_opposing_creature')
  else:
   for i in range(n):b.add('Grizzly Bears' if i%2==0 else 'Ashcoat Bear','grave_creature_'+str(i),side,'graveyard')
  for i,name in enumerate(('Forest','Giant Growth','Bone Saw')):b.add(name,'noncreature_grave_'+str(i),owner,'graveyard')
  if mode=='purged_input':b.purge()
 if mode=='tokens_do_not_contribute':
  component=data['witness_token_components']['Raise the Alarm'];b.spell('Raise the Alarm','raise_tokens');b.create(component['token'],component['count']);b.proof_tokens()
  if basis in ('own_grave_nontoken_deaths_this_turn','creature_cards_in_own_graveyard') and p['event']!='death':b.wrath(p)
 if mode=='wrath_death':b.add('Grizzly Bears','simultaneous_creature',owner,'battlefield')
 if mode=='reentry_changed_input':
  if basis=='devotion_white':b.add('Suntail Hawk','new_white',owner)
  elif basis!='controlled_lands':b.add('Ashcoat Bear','increment_creature',owner,'battlefield')


def variable_token_combat(b,p,mode):
 own=b.owner;definition=p['token'];live=[t for t in b.tokens if t['alive'] and t['player']==own and t['name']==definition['name']];assert live;chosen=live[0];tid=b.rows[definition['name']]['csvid'];b.emit(_variable_step('bind_permanent_copy',player=own,value=tid,index=0,ref='test_token'))
 if mode=='ready_attack':
  b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'));b.serial+=2
 defending=mode.startswith('block_');witness='Suntail Hawk' if mode=='block_flying' else 'Grizzly Bears' if defending else None
 if witness:b.add(witness,'combat_witness',1-own,'battlefield')
 attacker='combat_witness' if defending else 'test_token';blocker='test_token' if defending else 'combat_witness';can_attack=defending or mode=='ready_attack';blocks=defending and (mode!='block_flying' or bool({'Flying','Reach'} & set(chosen['keywords'])))
 b.emit(_variable_step('turn',player=1-own if defending else own,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref=attacker,value=int(can_attack)),_variable_step('finish_attackers'),_variable_step('begin_blockers'))
 if defending:b.emit(_variable_step('block',ref=blocker,other=attacker,value=int(blocks)));b.entities['combat_witness']['tapped']=True
 b.emit(_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'))
 if blocks:
  w=b.entities['combat_witness'];power=chosen['power']+chosen['counters'];toughness=chosen['toughness']+chosen['counters']
  if power>=w['toughness']:b.move('combat_witness','graveyard')
  else:w['damage']=power
  if w['power']>=toughness:b.drop_token(chosen)
  else:chosen['damage']=w['power']
 elif can_attack:
  if defending:b.life[own]-=b.entities['combat_witness']['power']
  else:b.life[1-own]-=chosen['power']+chosen['counters'];chosen['tapped']=True


def expand_variable_component(name,csvid,d,p,selected,rows,oracle):
 data=variable_data();validate_variable_witnesses(oracle,rows,p,data);allowed=variable_variants(p)
 if not selected or len(set(selected))!=len(selected) or any(v not in allowed for v in selected):raise ValueError('unique declared draft variable_variants required')
 result=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','event_snapshot':'create','landfall_scope_snapshot':'landfall_scope'}.get(variant,variant);b=VariableTokenLedger(owner,oracle,rows,data);b.add(name,'source');b.universe.add(p['token']['name'])
  for player,library in [(0,['Grizzly Bears','Forest','Island']),(1,['Ashcoat Bear','Plains','Mountain'])]:
   for i,nm in enumerate(library):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1])
  if p['basis']=='own_grave_nontoken_deaths_this_turn':b.emit(_variable_step('reset_death_history'))
  ready_cut=len(b.steps);prepare_variable_inputs(b,p,mode,data);event_cut=len(b.steps);source_amount=0
  if mode in ('payment','insufficient_mana','wrong_color'):
   pool=variable_cost(oracle[name]) if mode!='insufficient_mana' else [0]*6
   if mode=='wrong_color':
    color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   b.pool(owner,pool)
  denied=mode in ('insufficient_mana','wrong_color')
  if denied:b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  elif mode=='countered':
   b.add('Counterspell','counter',0);b.emit(_variable_step('choose_priority',ref='counter'),_variable_step('cast',ref='source',value=0),_variable_step('resolve'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[name]));b.pay(0,variable_cost(oracle['Counterspell']));b.move('source','graveyard');b.move('counter','graveyard')
  else:
   b.cast_source(p)
   if p['event']=='entry':source_amount=b.quantity(p)
   if p['event']=='death':
    b.emit(_variable_step('assert_zone_multiplicity',player=owner,zone='battlefield',index=rows[p['token']['name']]['csvid'],value=0));event_cut=len(b.steps)
    if mode=='bounce_source':b.spell('Unsummon','source_bounce','source');b.move('source','hand');b.emit(_variable_step('bind_hand',player=owner,value=csvid,ref='source'))
    elif mode=='exile_source':b.spell('Swords to Plowshares','source_exile','source');b.life[owner]+=int(oracle[name]['power']);b.move('source','exile')
    elif mode in ('wrath_death','tokens_do_not_contribute'):b.wrath(p,source_dies=True);source_amount=b.quantity(p)
    else:b.destroy('source');source_amount=b.quantity(p);b.source_event(p)
   b.proof_tokens() # positive amount/body receipt before cleanup or removals
   if mode=='source_removed':b.destroy('source')
   if mode=='reentry_changed_input':
    b.spell('Unsummon','bounce_for_reentry','source');b.move('source','hand');b.emit(_variable_step('bind_hand',player=owner,value=csvid,ref='source'))
    if p['basis']=='controlled_lands':b.atlas('atlas','next_land',p=p)
    elif p['basis']=='devotion_white':
     b.emit(_variable_step('cast',ref='new_white',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle['Suntail Hawk']));b.move('new_white','battlefield')
    else:b.destroy('increment_creature')
    b.cast_source(p);b.proof_tokens()
   if mode=='amount_locked':
    if p['basis']=='controlled_lands':b.destroy('counting_land_0')
    elif p['basis']=='devotion_white':
     for ref in list(b.entities):
      if ref.startswith('white_'):b.destroy(ref)
    elif p['basis']=='own_grave_nontoken_deaths_this_turn':b.emit(_variable_step('advance_turn'));b.turn=1-b.turn;b.serial+=1
    else:b.purge()
   if mode=='ownership' and p['basis']=='controlled_lands':b.destroy('foreign_land');b.destroy('owned_opposing_land')
   if mode=='ownership' and p['basis']=='devotion_white':b.destroy('foreign_white');b.destroy('owned_opposing_white')
   if mode in ('fresh_attack','ready_attack','block_ground','block_flying'):variable_token_combat(b,p,mode)
   if mode in ('destroy_token','bounce_token','exile_token'):
    target=next(t for t in b.tokens if t['alive'] and t['name']==p['token']['name'] and t['player']==owner);b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[p['token']['name']]['csvid'],index=0,ref='test_token'))
    if mode=='destroy_token':b.emit(_variable_step('destroy',ref='test_token'),_variable_step('refresh'))
    else:
     spell='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares';b.spell(spell,'token_removal','test_token')
     if mode=='exile_token':b.life[owner]+=target['power']+target['counters']
    b.drop_token(target,dies=mode=='destroy_token')
   if mode.startswith('landfall_') or mode=='atlas_tapped_denied':
    if mode=='landfall_source_removed':b.destroy('source')
    land_snapshot_cut=len(b.steps)
    if mode=='landfall_opponent':
     # garden_atlas already tapped: use a distinct ready opposing Atlas.
     b.add('Walking Atlas','other_land_atlas',1-owner,'battlefield');b.add('Forest','other_next_land',1-owner);b.atlas('other_land_atlas','other_next_land',1-owner,p=p)
    else:b.atlas('atlas','next_land',accept=mode!='landfall_decline',p=p)
    if mode=='landfall_repeat':b.atlas('second_atlas','second_land',p=p)
    if mode=='atlas_tapped_denied':b.emit(_variable_step('require_can_activate',ref='atlas',value=0),_variable_step('activate',ref='atlas',value=0))
    if mode=='landfall_cleanup':b.proof_tokens();b.emit(_variable_step('cleanup'),_variable_step('refresh'))
   if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant in ('ready_snapshot','pending_snapshot','event_snapshot','landfall_scope_snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else land_snapshot_cut if variant=='landfall_scope_snapshot' else event_cut;assert cut is not None;tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['separate entry/death-trigger priority responses','copy effects and token doublers','multiple opposing players','source control/ownership changes before its trigger','multiple blockers and damage assignment choices','source attack/block keywords except Caller Flash opposing-turn expiry case']+['inactive: '+x for x in p['inactive_clauses']]
  if p['mandatory_followups']:gaps+=['AI optional-decline policy; explicit legal decline is human0 only','nontoken Plant recipients and live type changes','control changes after counters have been applied']
  if p['event']=='death':gaps+=['graveyard changes between source death and trigger resolution; current destroy/resolve primitives drain that window']
  result.append(dict(name='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return result


class NoncreatureTokenLedger(VariableTokenLedger):
 """Actual count-spell resolution and permanent activations, independently modeled."""
 def create(self,definition,amount,player=None,origin='source'):
  start=len(self.tokens);super().create(definition,amount,player,origin)
  if getattr(self,'breadth_static',None):
   for token in self.tokens[start:]:token['printed_keywords']=list(token['keywords'])
   self.refresh_breadth_grant()
 def move(self,ref,zone):
  super().move(ref,zone)
  if getattr(self,'breadth_static',None):self.refresh_breadth_grant()
 def refresh_breadth_grant(self):
  live='source' in self.entities and self.entities['source']['zone']=='battlefield'
  for token in self.tokens:
   token['keywords']=list(token['printed_keywords'])
   if live and 'Soldier' in token['subtypes'] and 'Vigilance' not in token['keywords']:token['keywords'].append('Vigilance')
 def quantity(self,p):
  if p['basis']=='fixed':
   condition=p.get('empire_replacement')
   if condition and all(any(e['zone']=='battlefield' and e['player']==self.owner and e['name']==n for e in self.entities.values()) for n in condition['required_artifacts']):return condition['count']
   return p['fixed_count']
  if p['basis']=='controlled_creatures':return sum(e['zone']=='battlefield' and e['player']==self.owner and e['creature'] for e in self.entities.values())+sum(t['alive'] and t['player']==self.owner for t in self.tokens)
  if p['basis']=='instant_sorcery_cards_in_own_graveyard':return sum(any(k in self.oracle[n]['type_line'] for k in ('Instant','Sorcery')) for n in self.zones['graveyard'][self.owner])
  return super().quantity(p)
 def source_effect(self,p):
  start=len(self.tokens);self.create(p['token'],self.quantity(p))
  for token in self.tokens[start:]:token['tapped']=p['entry_tapped']
 def normal_mode(self,name):
  if name=='Saproling Symbiosis' and self.owner==0 and sum(self.mana[0])-sum(variable_cost(self.oracle[name]))>=2:self.emit(_variable_step('choose_dialog',player=0,value=0))
 def cast_maker(self,p,ref='source',response=False):
  player=self.owner;name=self.entities[ref]['name'];before=self.zones['hand'][player].count(name)
  self.normal_mode(name)
  self.emit(_variable_step('cast',ref=ref,value=1));self.pending_cut=len(self.steps);self.pay(player,variable_cost(self.oracle[name]));self.emit(_variable_step('assert_stack',value=1),_variable_step('assert_zone_multiplicity',player=player,zone='hand',index=self.rows[name]['csvid'],value=before),*(_variable_step('assert_mana',player=player,color=i,value=v) for i,v in enumerate(self.mana[player])))
  if response:
   spell='Murder' if p['basis']=='creature_cards_in_own_graveyard' else 'Unsummon' if p['basis']=='instant_sorcery_cards_in_own_graveyard' else 'Raise the Alarm';self.add(spell,'response')
   if spell!='Raise the Alarm':self.emit(_variable_step('choose_target',player=player,ref='response_witness'))
   self.emit(_variable_step('cast_response',ref='response',value=1),_variable_step('assert_stack',value=2),_variable_step('resolve_response'),_variable_step('refresh'),_variable_step('assert_stack',value=0));self.pay(player,variable_cost(self.oracle[spell]));self.move('response','graveyard')
   if spell=='Murder':self.move('response_witness','graveyard')
   elif spell=='Unsummon':self.move('response_witness','hand')
   else:
    component=self.data['witness_token_components'][spell];self.create(component['token'],component['count'],origin='independent')
  else:self.emit(_variable_step('resolve'),_variable_step('refresh'))
  self.emit(_variable_step('assert_choices_consumed'))
  if p['activation'] is None:self.source_effect(p);self.move(ref,'graveyard')
  else:
   self.move(ref,'battlefield')
   if p.get('entry_creation'):self.create(p['token'],p['entry_creation']['count'])
 def activate_maker(self,p,remove_pending=False,remove_piece_pending=False):
  a=p['activation'];self.emit(_variable_step('require_can_activate',ref='source',value=1))
  if p['source_name']=='Myr Turbine' and self.owner==1:self.emit(_variable_step('choose_dialog',player=1,value=0))
  self.emit(_variable_step('activate',ref='source',value=1));self.activation_cut=len(self.steps);self.pay(self.owner,a['mana'])
  if a['tap']:self.entities['source']['tapped']=True
  if a['sacrifice']:
   self.move('source','graveyard');self.emit(_variable_observe('source','in_play',0),_variable_step('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=self.rows[p['source_name']]['csvid'],value=1))
  self.emit(_variable_step('assert_stack',value=1),*(_variable_step('assert_mana',player=self.owner,color=i,value=v) for i,v in enumerate(self.mana[self.owner])));self.proof_tokens()
  if remove_pending:self.destroy('source')
  if remove_piece_pending:self.destroy('crown_piece')
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.source_effect(p)
 def actual_untap(self):
  # The full wrapper rejects preloaded pools and requires a freshly prepared
  # advance_turn boundary. Never insert the synthetic turn setter here.
  self.pool(0,[0]*6);self.pool(1,[0]*6)
  self.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('untap_step',player=self.owner));self.serial+=2
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==self.owner:e['tapped']=False
  for token in self.tokens:
   if token['alive'] and token['player']==self.owner:token['tapped']=False


def noncreature_prepare(b,p,mode):
 owner=b.owner;basis=p['basis'];n=0 if mode=='minimum' else 4 if mode=='larger_amount' else 2;side=1-owner if mode=='opponent_state' else owner
 if basis in ('creature_cards_in_own_graveyard','instant_sorcery_cards_in_own_graveyard'):
  counted=['Grizzly Bears']*n if basis=='creature_cards_in_own_graveyard' else ['Giant Growth' if i%2 else 'Unsummon' for i in range(n)]
  for i,name in enumerate(counted):b.add(name,'count_input_'+str(i),side,'graveyard')
  for i,name in enumerate(['Forest','Bone Saw']+(['Giant Growth'] if basis=='creature_cards_in_own_graveyard' else ['Grizzly Bears'])):b.add(name,'excluded_input_'+str(i),owner,'graveyard')
 elif basis=='controlled_creatures':
  if mode=='ownership':
   b.add('Grizzly Bears','foreign_controlled',owner,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing',1-owner,'battlefield',opponent_owned=True)
  else:
   for i in range(n):b.add('Grizzly Bears','count_input_'+str(i),side,'battlefield')
  b.add('Bone Saw','noncreature_control',owner,'battlefield')
 if mode=='ownership' and basis!='controlled_creatures':
  # Actual foreign and domestic destinations distinguish card ownership from
  # its former controller; these deaths are not fabricated graveyard flags.
  b.add('Grizzly Bears','foreign_controlled',owner,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing',1-owner,'battlefield',opponent_owned=True);b.destroy('foreign_controlled');b.destroy('owned_opposing')
 if mode=='token_input':
  b.spell('Raise the Alarm','token_input_spell');component=b.data['witness_token_components']['Raise the Alarm'];b.create(component['token'],component['count'],origin='independent');b.proof_tokens()
  if basis!='controlled_creatures':
   b.wrath(p)
 if mode=='purged_input':
  if basis=='controlled_creatures':b.wrath(p)
  else:b.purge()
 if mode in ('response_count','response_snapshot'):b.add('Grizzly Bears','response_witness',owner,'battlefield')


def noncreature_combat(b,p,mode):
 own=b.owner;chosen=next(t for t in b.tokens if t['alive'] and t['player']==own and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=own,value=b.rows[chosen['name']]['csvid'],index=0,ref='test_token'))
 if mode in ('ready_attack','flying_evasion') or mode.startswith('untap_block'):b.actual_untap()
 defending='block' in mode;flying=mode.endswith('flying');witness='Suntail Hawk' if flying else 'Grizzly Bears'
 if defending or mode=='flying_evasion':b.add(witness,'combat_witness',1-own,'battlefield')
 blocks=defending and not chosen['tapped'] and (not flying or bool({'Flying','Reach'} & set(chosen['keywords'])));can_attack=defending or (mode in ('ready_attack','flying_evasion') and not chosen['tapped']);attacker='combat_witness' if defending else 'test_token'
 b.emit(_variable_step('turn',player=1-own if defending else own,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref=attacker,value=int(can_attack)),_variable_step('finish_attackers'),_variable_step('begin_blockers'))
 if defending:b.emit(_variable_step('block',ref='test_token',other=attacker,value=int(blocks)));b.entities['combat_witness']['tapped']=True
 if mode=='flying_evasion':b.emit(_variable_step('block',ref='combat_witness',other=attacker,value=0))
 b.emit(_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'))
 if blocks:
  w=b.entities['combat_witness']
  if chosen['power']>=w['toughness']:b.move('combat_witness','graveyard')
  else:w['damage']=chosen['power']
  if w['power']>=chosen['toughness']:b.drop_token(chosen)
  else:chosen['damage']=w['power']
 elif can_attack:
  if defending:b.life[own]-=b.entities['combat_witness']['power']
  else:b.life[1-own]-=chosen['power'];chosen['tapped']='Vigilance' not in chosen['keywords']


def expand_noncreature_component(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import noncreature_variants
 data=variable_data();validate_variable_witnesses(oracle,rows,p,data);allowed=noncreature_variants(p)
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in allowed for v in selected):raise ValueError('unique supported noncreature variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','effect_snapshot':'create','activation_snapshot':'create','untap_snapshot':'untap_repeat' if p['activation'] else 'untap_block_ground','response_snapshot':'response_count','empire_condition_snapshot':'empire_pair'}.get(variant,variant);b=NoncreatureTokenLedger(owner,oracle,rows,data);b.breadth_static=p.get('active_static');b.add(name,'source');b.universe.add(p['token']['name'])
  for player,library in [(0,['Grizzly Bears','Forest','Island']),(1,['Ashcoat Bear','Plains','Mountain'])]:
   for i,nm in enumerate(library):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps)
  if p['activation'] is None:noncreature_prepare(b,p,mode)
  if p.get('empire_replacement') and mode.startswith('empire_'):prepare_empire_inputs(b,p,mode)
  if mode=='two_casts':b.add(name,'second')
  if mode in ('payment','insufficient_mana','wrong_color'):
   pool=variable_cost(oracle[name]) if mode!='insufficient_mana' else [0]*6
   if mode=='wrong_color':
    color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   b.pool(owner,pool)
  if mode in ('insufficient_mana','wrong_color'):b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  elif mode=='countered':
   b.add('Counterspell','counter',0);b.normal_mode(name);b.emit(_variable_step('choose_priority',ref='counter'),_variable_step('cast',ref='source',value=0),_variable_step('resolve'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[name]));b.pay(0,variable_cost(oracle['Counterspell']));b.move('source','graveyard');b.move('counter','graveyard')
  else:
   b.cast_maker(p,response=mode=='response_count');effect_cut=len(b.steps)
   if p['activation']:
    b.emit(_variable_step('assert_zone_multiplicity',player=owner,zone='battlefield',index=rows[p['token']['name']]['csvid'],value=p.get('entry_creation',{}).get('count',0)))
    if mode=='empire_one_lost':b.destroy('crown_piece')
    if mode not in ('cast_only','payment'):
     if mode in ('activation_payment','activation_insufficient','activation_wrong_color'):
      pool=list(p['activation']['mana']) if mode!='activation_insufficient' else [0]*6
      if mode=='activation_wrong_color':
       color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
      b.pool(owner,pool)
     if mode in ('activation_insufficient','activation_wrong_color'):b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0))
     else:b.activate_maker(p,remove_pending=mode=='source_removed_pending',remove_piece_pending=mode=='empire_lost_pending')
   b.proof_tokens()
   if mode=='two_casts':b.cast_maker(p,'second');b.proof_tokens()
   if mode=='source_removed':b.destroy('source')
   if mode=='amount_locked':
    if p['basis']=='controlled_creatures':
     for ref in list(b.entities):
      if ref.startswith('count_input_'):b.destroy(ref)
    else:b.purge()
   if mode=='repeat':
    if p['activation']['tap'] or p['activation']['sacrifice']:b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0))
    else:b.activate_maker(p)
   if mode=='tapped_denied':b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0))
   if mode=='untap_repeat':
    untap_cut=len(b.steps);b.actual_untap();b.emit(_variable_step('turn',player=owner,value=0));b.pool(owner,p['activation']['mana']);b.activate_maker(p)
   if mode in ('fresh_attack','ready_attack','block_ground','block_flying','untap_block_ground','untap_block_flying','flying_evasion'):
    untap_cut=len(b.steps);noncreature_combat(b,p,mode)
   if mode in ('destroy_token','bounce_token','exile_token'):
    token=next(t for t in b.tokens if t['alive'] and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[p['token']['name']]['csvid'],index=0,ref='test_token'))
    if mode=='destroy_token':b.emit(_variable_step('destroy',ref='test_token'),_variable_step('refresh'))
    else:
     spell='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares';b.spell(spell,'token_removal','test_token')
     if mode=='exile_token':b.life[owner]+=token['power']
    b.drop_token(token,dies=mode=='destroy_token')
   if mode in ('grant_scope','grant_lost'):mobilization_scope(b,p,removed=mode=='grant_lost')
   if mode=='empire_pair_cleanup':b.destroy('crown_piece');b.emit(_variable_step('cleanup'),_variable_step('refresh'))
   if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant in ('pending_snapshot','response_snapshot') else b.activation_cut if variant=='activation_snapshot' else untap_cut if variant=='untap_snapshot' else effect_cut;tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['inactive: '+line for line in p['inactive_clauses']]+['token doublers, replacement effects, control changes and multiplayer','normal cost/main-phase source casting; alternate costs and graveyard casting','multiple blockers and damage assignment choices']
  if p['activation']:gaps+=['countering the activated ability; source is sacrificed as its printed cost before resolution' if p['activation']['sacrifice'] else 'countering the activated ability; card destruction while pending is covered']
  out.append(dict(name='TokenSpellExtensionRecipe_'+str(csvid)+'_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out




# Fixed token creation from actual Oracle-selected death events.
def watch_victim(b,ref):
 e=copy.deepcopy(b.entities[ref]);e.update(ref=ref,token=False,subtypes=b.oracle[e['name']]['type_line'].split(' — ',1)[-1].split());return e

def watch_matches(b,p,v):
 predicate=p['predicate'];same=v.get('ref')=='source';creature=v.get('creature',True);nt=not v['token'];controlled=v['player']==b.owner;owned=v['owner']==b.owner;types=v['subtypes']
 if predicate=='owner_grave_nontoken_creature':return nt and creature and owned
 if predicate=='controller_nontoken_creature':return nt and creature and controlled
 if predicate=='another_controller_nonspirit_creature':return not same and creature and controlled and 'Spirit' not in types
 if predicate=='self_or_controller_nontoken_creature':return same or (nt and creature and controlled)
 if predicate=='another_controller_nontoken_creature':return not same and nt and creature and controlled
 if predicate=='self_or_global_cleric':return same or (creature and 'Cleric' in types)
 if predicate=='another_owner_grave_nontoken_elf':return not same and nt and owned and 'Elf' in types
 raise ValueError('unreviewed watch predicate')

class DeathWatchLedger(VariableTokenLedger):
 def watch_count(self,p,victims):return sum(watch_matches(self,p,v) for v in victims) if self.entities['source']['zone']=='battlefield' else 0
 def prepare_watch(self,p,victims,accept=True):
  amount=self.watch_count(p,victims)
  if amount and p['optional'] and self.owner==0 and accept:
   if any(v.get('ref')=='source' for v in victims):self.emit(_variable_step('choose_trigger',player=0,ref='source'))
   else:self.emit(_variable_step('choose_priority',ref='source'))
  return amount if accept else 0
 def destroy_watched(self,p,ref,accept=True):
  n=self.prepare_watch(p,[watch_victim(self,ref)],accept);self.destroy(ref);self.create(p['token'],n)
 def spell_death(self,p,ref):
  self.add('Murder','lethal_spell');self.emit(_variable_step('choose_target',player=self.owner,ref=ref));n=self.prepare_watch(p,[watch_victim(self,ref)])
  self.emit(_variable_step('cast',ref='lethal_spell',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(self.owner,variable_cost(self.oracle['Murder']));self.move('lethal_spell','graveyard');self.move(ref,'graveyard');self.create(p['token'],n)
 def wrath_watched(self,p):
  refs=[ref for ref,e in self.entities.items() if e['zone']=='battlefield' and e['creature']];tokens=[t for t in self.tokens if t['alive']];victims=[watch_victim(self,r) for r in refs]+[dict(t,creature=True,token=True) for t in tokens];n=self.prepare_watch(p,victims)
  # Last-known source presence is captured before the simultaneous death;
  # the printed token event occurs after all old creatures enter graveyards.
  self.spell('Wrath of God','wrath')
  for ref in refs:self.move(ref,'graveyard')
  for token in tokens:self.drop_token(token)
  self.create(p['token'],n)
 def destroy_watched_token(self,p,token,ref='test_token'):
  n=self.prepare_watch(p,[dict(token,creature=True,token=True)]);self.emit(_variable_step('destroy',ref=ref),_variable_step('refresh'));self.drop_token(token);self.create(p['token'],n)
 def mana_token(self,p):
  token=next(t for t in self.tokens if t['alive'] and t['origin']=='source');tid=self.rows[token['name']]['csvid'];self.emit(_variable_step('bind_permanent_copy',player=self.owner,value=tid,index=0,ref='mana_token'),_variable_step('require_can_activate',ref='mana_token',value=1),_variable_step('activate',ref='mana_token',value=1),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'),_variable_observe('mana_token','in_play',0));self.drop_token(token);self.mana[self.owner][0]+=1

def watch_source_flying(b,p):
 b.add('Grizzly Bears','ground_blocker',1-b.owner,'battlefield');b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('turn',player=b.owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('block',ref='ground_blocker',other='source',value=0),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.life[1-b.owner]-=p['power'];b.entities['source']['tapped']=True

def expand_death_watch(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import death_watch_variants
 data=variable_data();validate_variable_witnesses(oracle,rows,p,data);allowed=death_watch_variants(p)
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in allowed for v in selected):raise ValueError('unique explicit death watch variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','event_snapshot':'create','wrath_snapshot':'wrath_death','token_mana_snapshot':'token_mana'}.get(variant,variant);b=DeathWatchLedger(owner,oracle,rows,data);b.add(name,'source');b.universe.add(p['token']['name']);victim='Elvish Mystic' if p['predicate']=='another_owner_grave_nontoken_elf' else 'Benevolent Bodyguard' if p['predicate']=='self_or_global_cleric' else 'Grizzly Bears'
  if mode=='noncreature_death':victim='Bone Saw'
  if mode=='other_type':victim='Lantern Kami' if p['predicate']=='another_controller_nonspirit_creature' else 'Grizzly Bears'
  side=1-owner if mode in ('opponent_death','owned_opposing') else owner;foreign=mode in ('foreign_controlled','owned_opposing');b.add(victim,'victim',side,'battlefield',opponent_owned=foreign)
  if mode in ('repeat','source_removed','source_bounced','source_exiled','wrath_death'):b.add(victim,'second_victim',owner,'battlefield')
  if mode=='wrath_death':b.add(victim,'opposing_victim',1-owner,'battlefield')
  if mode=='kindred_elf_death':b.add('Prowess of the Fair','kindred_elf')
  for player,library in [(0,['Grizzly Bears','Forest','Island']),(1,['Ashcoat Bear','Plains','Mountain'])]:
   for i,nm in enumerate(library):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps)
  if mode=='prior_death':b.destroy('victim')
  if mode in ('payment','insufficient_mana','wrong_color'):
   for ref,e in b.entities.items():
    if e['zone']=='battlefield' and e['player']==owner and e['name']=='Elvish Mystic':e['tapped']=True;next(c for c in b.setup if c['ref']==ref)['state']=['tapped']
   pool=variable_cost(oracle[name]) if mode!='insufficient_mana' else [0]*6
   if mode=='wrong_color':
    color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   b.pool(owner,pool)
  if mode in ('insufficient_mana','wrong_color'):b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  elif mode=='countered':
   b.add('Counterspell','counter',0);b.emit(_variable_step('choose_priority',ref='counter'),_variable_step('cast',ref='source',value=0),_variable_step('resolve'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[name]));b.pay(0,variable_cost(oracle['Counterspell']));b.move('source','graveyard');b.move('counter','graveyard')
  else:
   b.cast_source(p);b.emit(_variable_step('assert_zone_multiplicity',player=owner,zone='battlefield',index=rows[p['token']['name']]['csvid'],value=0),_variable_step('assert_matching_permanents',player=owner,index=csvid,property='color',value=sum(1<<VARIABLE_COLOR_INDEX[c] for c in oracle[name]['colors'])));event_cut=len(b.steps)
   if mode in ('cast_only','payment','prior_death'):pass
   elif mode=='source_keyword_combat':watch_source_flying(b,p)
   elif mode=='bounce_only':b.spell('Unsummon','nondeath_bounce','victim');b.move('victim','hand')
   elif mode=='exile_only':b.spell('Swords to Plowshares','nondeath_exile','victim');b.life[side]+=b.entities['victim']['power'];b.move('victim','exile')
   elif mode=='source_death':b.destroy_watched(p,'source')
   elif mode=='wrath_death':b.wrath_watched(p)
   elif mode=='independent_token_death':
    b.spell('Raise the Alarm','independent_token_spell');component=data['witness_token_components']['Raise the Alarm'];b.create(component['token'],component['count'],origin='independent');b.proof_tokens();token=next(t for t in b.tokens if t['alive'] and t['origin']=='independent');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='independent_token'));b.destroy_watched_token(p,token,'independent_token')
   elif mode=='kindred_elf_death':
    b.cast_source(p,'kindred_elf');b.emit(_variable_step('assert_matching_permanents',player=owner,index=csvid,property='subtype_Elf',value=1));b.destroy_watched(p,'kindred_elf')
   else:
    if mode=='murder':b.spell_death(p,'victim')
    else:b.destroy_watched(p,'victim',accept=mode!='decline')
   b.proof_tokens() # exact positive source event before cleanup/loss/removal
   if mode=='repeat':b.destroy_watched(p,'second_victim')
   if mode in ('source_removed','source_bounced','source_exiled'):
    if mode=='source_removed':b.destroy_watched(p,'source')
    elif mode=='source_bounced':b.spell('Unsummon' if p['creature'] else 'Boomerang','source_removal','source');b.move('source','hand')
    else:
     # Universal exile witness avoids restricting the source's card type.
     b.spell('Revoke Existence' if not p['creature'] else 'Swords to Plowshares','source_removal','source');b.move('source','exile')
     if p['creature']:b.life[owner]+=p['power']
    if name=='Prowess of the Fair' and mode=='source_removed':b.emit(_variable_step('choose_dialog',player=owner,value=0))
    b.destroy_watched(p,'second_victim')
    if name=='Prowess of the Fair' and mode=='source_removed':b.emit(_variable_step('assert_unused_choices',value=1))
   if mode in ('fresh_attack','ready_attack','block_ground','block_flying'):variable_token_combat(b,p,mode)
   if mode in ('destroy_token','bounce_token','exile_token','spirit_token_negative'):
    token=next(t for t in b.tokens if t['alive'] and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[p['token']['name']]['csvid'],index=0,ref='test_token'))
    if mode in ('destroy_token','spirit_token_negative'):b.destroy_watched_token(p,token)
    else:
     spell='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares';b.spell(spell,'token_removal','test_token');b.drop_token(token,dies=False)
     if mode=='exile_token':b.life[owner]+=token['power']
   if mode=='token_mana':mana_cut=len(b.steps);b.mana_token(p)
   if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else mana_cut if variant=='token_mana_snapshot' else event_cut;tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['source control changes before its trigger','multiple simultaneous source copies except the Prowess kindred-Elf witness','independent token doubling, replacement effects and multiplayer','separate priority responses to death triggers; destroy/resolve drains current windows','source noncreature type changes and losing/gaining creature types']
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out


# Main-phase Twiddle effect components; untap timing is a
# negative Oracle control, not an invented upkeep/resolution entry point.
class InspiredTokenLedger(NoncreatureTokenLedger):
 def inspired_effect(self,p):self.create(p['token'],p['count'])
 def mana_receipt(self,player):self.emit(*(_variable_step('assert_mana',player=player,color=i,value=v) for i,v in enumerate(self.mana[player])))
 def twiddle(self,p,target,tap=False,pay=False,unexpected_sentinel=False,accept=True):
  ref='twiddle_'+str(sum(e['name']=='Twiddle' for e in self.entities.values()));self.add('Twiddle',ref,0);self.emit(_variable_step('choose_target',player=0,ref=target),_variable_step('choose_dialog',player=0,value=0 if tap else 1))
  if pay or unexpected_sentinel:self.emit(_variable_step('choose_dialog',player=self.owner,value=0 if accept else 1))
  self.emit(_variable_step('cast',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'));self.pay(0,variable_cost(self.oracle['Twiddle']));self.move(ref,'graveyard');self.entities[target]['tapped']=tap
  if pay and accept:self.pay(self.owner,p['payment']);self.inspired_effect(p)
  if unexpected_sentinel:self.emit(_variable_step('assert_unused_choices',value=1))
  self.emit(_variable_step('assert_choices_consumed'));self.mana_receipt(0)
  if self.owner!=0:self.mana_receipt(self.owner)
 def untap_timing(self,p,opponent=False):
  self.pool(0,[0]*6);self.pool(1,[0]*6);player=1-self.owner if opponent else self.owner;self.emit(_variable_step('advance_turn'))
  if not opponent:self.emit(_variable_step('advance_turn'))
  if not opponent:self.emit(_variable_step('choose_dialog',player=self.owner,value=1))
  self.emit(_variable_step('untap_step',player=player),_variable_step('refresh'))
  if not opponent:self.emit(_variable_step('assert_unused_choices',value=1))
  self.emit(_variable_step('assert_choices_consumed'))
  for e in self.entities.values():
   if e['zone']=='battlefield' and e['player']==player:e['tapped']=False
  # Oracle: untap-step triggers wait for upkeep. No payment/token is modeled.
  self.emit(_variable_step('assert_stack',value=0));self.mana_receipt(0);self.mana_receipt(1)

def inspired_combat(b,p,mode):
 if mode=='fresh_attack' and 'Haste' in p['token']['keywords']:
  token=next(t for t in b.tokens if t['alive'] and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=b.owner,value=b.rows[token['name']]['csvid'],index=0,ref='test_token'),_variable_step('turn',player=b.owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='test_token',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));token['tapped']=True;b.life[1-b.owner]-=token['power']
 elif mode=='flying_evasion':
  # Ready via the established real turn gate; no extra untap event is inserted.
  b.add('Grizzly Bears','ground_blocker',1-b.owner,'battlefield');start=len(b.steps);variable_token_combat(b,p,'ready_attack');idx=next(i for i in range(start,len(b.steps)) if b.steps[i]['op']=='advance_combat');b.steps.insert(idx,_variable_step('block',ref='ground_blocker',other='test_token',value=0))
 else:variable_token_combat(b,p,mode)

def inspired_source_combat(b,p,mode):
 own=b.owner;fresh=mode=='source_haste';witness='Walking Atlas' if mode.endswith('artifact') else 'Walking Corpse' if mode.endswith('black') else 'Grizzly Bears';blocked=not fresh and mode!='source_intimidate_ground'
 if not fresh:b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'));b.add(witness,'source_blocker',1-own,'battlefield')
 b.emit(_variable_step('turn',player=own,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'))
 if not fresh:b.emit(_variable_step('block',ref='source_blocker',other='source',value=int(blocked)))
 b.emit(_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.entities['source']['tapped']=True
 if blocked:
  w=b.entities['source_blocker']
  if p['power']>=w['toughness']:b.move('source_blocker','graveyard')
  else:w['damage']=p['power']
  if w['power']>=p['toughness']:b.move('source','graveyard')
  else:b.entities['source']['damage']=w['power']
 else:b.life[1-own]-=p['power']

def expand_inspired(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import inspired_variants
 data=variable_data();validate_variable_witnesses(oracle,rows,p,data);allowed=inspired_variants(p)
 if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in allowed for v in selected):raise ValueError('unique supported inspired variants required')
 out=[];land={'B':'Swamp','U':'Island','G':'Forest','R':'Mountain','W':'Plains'};paycolor=next(c for c in VARIABLE_COLOR_INDEX if p['payment'][VARIABLE_COLOR_INDEX[c]])
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','effect_snapshot':'create','repeat_snapshot':'repeat'}.get(variant,variant);b=InspiredTokenLedger(owner,oracle,rows,data)
  # These lands enable a real premature-payment timing control. Normal
  # functional main-phase effects use the explicitly receipted printed pool.
  for i in range(3):b.add(land[paycolor],'payment_land_'+str(i),owner,'battlefield',tapped=True)
  b.add(name,'source');b.universe.add(p['token']['name']);b.add('Grizzly Bears','other_untap',owner,'battlefield',tapped=True);b.add('Ashcoat Bear','opponent_untap',1-owner,'battlefield',tapped=True)
  if mode=='enchantment_type':b.add('Disenchant','type_witness')
  for player,library in [(0,['Grizzly Bears','Forest','Island']),(1,['Ashcoat Bear','Plains','Mountain'])]:
   for i,nm in enumerate(library):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps)
  if mode in ('source_payment','source_insufficient','source_wrong_color'):
   pool=variable_cost(oracle[name]) if mode!='source_insufficient' else [0]*6
   if mode=='source_wrong_color':
    color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   b.pool(owner,pool)
  if mode in ('source_insufficient','source_wrong_color'):b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  elif mode=='countered':
   b.add('Counterspell','counter',0);b.emit(_variable_step('choose_priority',ref='counter'),_variable_step('cast',ref='source',value=0),_variable_step('resolve'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[name]));b.pay(0,variable_cost(oracle['Counterspell']));b.move('source','graveyard');b.move('counter','graveyard')
  else:
   b.cast_source(p);b.emit(_variable_step('assert_zone_multiplicity',player=owner,zone='battlefield',index=rows[p['token']['name']]['csvid'],value=0))
   if mode=='enchantment_type':b.emit(_variable_step('require_can_cast',ref='type_witness',value=0))
   if mode in ('cast_only','source_payment'):pass
   elif mode.startswith('source_intimidate') or mode=='source_haste':inspired_source_combat(b,p,mode)
   elif mode=='other_untaps':b.twiddle(p,'other_untap')
   elif mode=='already_untapped':b.twiddle(p,'source',unexpected_sentinel=True)
   else:
    b.twiddle(p,'source',tap=True);event_cut=len(b.steps)
    if mode in ('untap_timing','opponent_untap'):b.untap_timing(p,opponent=mode=='opponent_untap')
    elif mode in ('source_destroyed','source_bounced','source_exiled'):
     if mode=='source_destroyed':b.destroy('source')
     elif mode=='source_bounced':b.spell('Unsummon','source_removal','source');b.move('source','hand')
     else:b.spell('Swords to Plowshares','source_removal','source');b.move('source','exile');b.life[owner]+=p['power']
     b.twiddle(p,'other_untap')
    else:
     if mode in ('payment','payment_insufficient','payment_wrong_color'):
      pool=list(p['payment'])
      if mode=='payment_insufficient':pool[0]-=1
      if mode=='payment_wrong_color':pool=[3,0,0,0,0,0]
      if owner==0:pool[2]+=1 # Twiddle itself is a separate real {U} spell.
      b.pool(owner,pool)
     b.twiddle(p,'source',pay=mode not in ('payment_insufficient','payment_wrong_color'),accept=mode!='decline');b.proof_tokens()
     if mode=='repeat':b.twiddle(p,'source',tap=True);b.twiddle(p,'source',pay=True)
     if mode in ('fresh_attack','ready_attack','block_ground','block_flying','flying_evasion'):inspired_combat(b,p,mode)
     if mode in ('destroy_token','bounce_token','exile_token','enchantment_type'):
      token=next(t for t in b.tokens if t['alive'] and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='test_token'))
      if mode=='destroy_token':b.emit(_variable_step('destroy',ref='test_token'),_variable_step('refresh'))
      else:
       spell='Disenchant' if mode=='enchantment_type' else 'Unsummon' if mode=='bounce_token' else 'Swords to Plowshares'
       if mode=='enchantment_type':
        b.emit(_variable_step('assert_stack',value=0),_variable_step('turn',player=owner,value=0),_variable_observe('test_token','in_play',1),_variable_step('require_can_cast',ref='type_witness',value=1),_variable_step('choose_target',player=owner,ref='test_token'),_variable_step('cast',ref='type_witness',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[spell]));b.move('type_witness','graveyard')
       else:b.spell(spell,'token_removal','test_token')
       if mode=='exile_token':b.life[owner]+=token['power']
      b.drop_token(token,dies=mode in ('destroy_token','enchantment_type'))
     if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else event_cut;tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['untap-step trigger placement and payment during the following real upkeep; timing control intentionally stops before upkeep','separate response priority after main-phase untapping and before inspired ability resolution; Twiddle resolve drains current windows','token doubling, replacement effects, source control changes, multiplayer and multiple blockers']
  out.append(dict(name='TokenTriggerCreationRecipe_'+str(csvid)+'_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out



def prepare_empire_inputs(b,p,mode):
 names=['Crown of Empires','Scepter of Empires']
 if mode=='empire_crown_only':names=names[:1]
 if mode=='empire_scepter_only':names=names[1:]
 side=1-b.owner if mode in ('empire_opponent','empire_owned_opposing') else b.owner
 foreign=mode in ('empire_foreign_controlled','empire_owned_opposing')
 for name in names:
  ref='crown_piece' if name=='Crown of Empires' else 'scepter_piece'
  if foreign:b.add(name,ref,side,'battlefield',opponent_owned=True)
  else:
   b.add(name,ref,side);b.emit(_variable_step('turn',player=side,value=0));b.cast_source(dict(event='watch'),ref)
 b.emit(_variable_step('turn',player=b.owner,value=0))

def mobilization_scope(b,p,removed=False):
 for player in (b.owner,1-b.owner):
  ref='alarm_'+str(player);b.spell('Raise the Alarm',ref,player=player);component=b.data['witness_token_components']['Raise the Alarm'];b.create(component['token'],component['count'],player,origin='independent')
  b.add('Infantry Veteran','normal_soldier_'+str(player),player,'battlefield')
 b.add('Grizzly Bears','ordinary_attacker',b.owner,'battlefield');b.proof_tokens()
 b.add('Disenchant','remove_grant')
 if removed:
  b.emit(_variable_step('choose_target',player=b.owner,ref='source'),_variable_step('cast',ref='remove_grant',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(b.owner,variable_cost(b.oracle['Disenchant']));b.move('remove_grant','graveyard');b.move('source','graveyard')
 b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'))
 for player in (b.owner,1-b.owner):
  token=next(t for t in b.tokens if t['alive'] and t['player']==player);ref='scope_token_'+str(player);b.emit(_variable_step('bind_permanent_copy',player=player,value=b.rows['Soldier']['csvid'],index=0,ref=ref),_variable_step('turn',player=player,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref=ref,value=1),_variable_step('attack',ref='normal_soldier_'+str(player),value=1))
  if player==b.owner:b.emit(_variable_step('attack',ref='ordinary_attacker',value=1));b.entities['ordinary_attacker']['tapped']=True
  b.emit(_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));token['tapped']=removed;b.entities['normal_soldier_'+str(player)]['tapped']=removed;b.life[1-player]-=2+(2 if player==b.owner else 0)

# Wave 6 private profile branch. Old ledgers/palettes are deliberately unchanged.
class Wave6TokenLedger(VariableTokenLedger):
 def __init__(self,owner,oracle,rows,p,data):
  super().__init__(owner,oracle,rows,data);self.profile=p;self.activation_cut=None;self.event_cut=None
 def refresh_grant(self):
  live=self.profile['mechanism']=='servo_lord' and self.entities.get('source',{}).get('zone')=='battlefield'
  for token in self.tokens:
   token.setdefault('printed_power',token['power']);token.setdefault('printed_toughness',token['toughness'])
   bonus=int(live and token['player']==self.owner and bool({'Servo','Thopter'}&set(token['subtypes'])))
   token['power']=token['printed_power']+bonus;token['toughness']=token['printed_toughness']+bonus
 def create(self,definition,amount,player=None,origin='source'):
  super().create(definition,amount,player,origin);self.refresh_grant()
 def move(self,ref,zone):
  super().move(ref,zone);self.refresh_grant()
 def cast_permanent(self):
  self.cast_source(dict(event='watch'));self.refresh_grant()
 def cast_creator(self,x=0,grave=False):
  from token_trigger_creation_spec import wave6_cost
  p=self.profile;ref='source';owner=self.owner;record=self.oracle[p['source_name']]
  if grave:
   self.emit(_variable_step('require_can_cast_graveyard',ref=ref,value=1),_variable_step('cast_graveyard',ref=ref,value=1),_variable_step('assert_stack',value=0),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(owner,[7,0,0,0,0,2]);self.move(ref,'exile')
  else:
   if '{X}' in record['mana_cost']:self.emit(_variable_step('choose_x',player=owner,value=x))
   self.emit(_variable_step('cast',ref=ref,value=1));self.pending_cut=len(self.steps);self.pay(owner,wave6_cost(record,x));self.emit(_variable_step('assert_stack',value=1),_variable_step('assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[p['source_name']]['csvid'],value=1));self.emit(*(_variable_step('assert_mana',player=owner,color=i,value=v) for i,v in enumerate(self.mana[owner])));self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.move(ref,'graveyard')
  if p['mechanism']=='x_wrath' and x>=5:
   for key,e in self.entities.items():
    if e['zone']=='battlefield' and e['creature']:self.move(key,'graveyard')
   self.kill_tokens()
  amount=10 if grave else p['amount'].get('normal',x)
  if p['mechanism']=='death_history':amount=sum(d['owner']==owner and d['serial']==self.serial for d in self.deaths)
  self.create(p['token'],amount)
 def activate_creator(self):
  p=self.profile;owner=self.owner;self.activation_cut=len(self.steps)
  if p['mechanism']=='land_activation' and owner==0:self.emit(_variable_step('choose_dialog',player=owner,value=1))
  self.emit(_variable_step('require_can_activate',ref='source',value=1),_variable_step('activate',ref='source',value=1),_variable_step('assert_stack',value=1));self.pay(owner,p['activation']['mana'])
  if p['activation']['tap']:self.entities['source']['tapped']=True
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.create(p['token'],1)
 def tap_spell(self,target='source',tap=True):
  ref='twiddle_'+str(sum(e['name']=='Twiddle' for e in self.entities.values()));self.add('Twiddle',ref,0);self.emit(_variable_step('choose_target',player=0,ref=target),_variable_step('choose_dialog',player=0,value=0 if tap else 1),_variable_step('cast',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(0,variable_cost(self.oracle['Twiddle']));self.move(ref,'graveyard')
  old=self.entities[target]['tapped'];self.entities[target]['tapped']=tap
  if target=='source' and tap and not old and self.entities[target]['zone']=='battlefield':self.create(self.profile['token'],1)
 def artifact_cast(self,name,ref,player=None,accept=True):
  player=self.owner if player is None else player
  triggers=player==self.owner and 'Artifact' in self.oracle[name]['type_line'] and self.entities['source']['zone']=='battlefield';pay=triggers and accept
  if pay and self.owner==0:self.emit(_variable_step('choose_priority',ref='source'))
  self.spell(name,ref,player=player)
  if any(t in self.oracle[name]['type_line'] for t in ('Artifact','Creature','Enchantment')):self.move(ref,'battlefield')
  if pay:self.pay(self.owner,[1,0,0,0,0,0]);self.create(self.profile['token'],1)
 def witness_tokens(self,name,ref,player=None):
  player=self.owner if player is None else player;component=self.data['witness_token_components'][name];self.spell(name,ref,player=player)
  if 'Creature' in self.oracle[name]['type_line']:self.move(ref,'battlefield')
  self.create(component['token'],component['count'],player,'independent')


def wave6_combat(b,p,mode):
 chosen=next(t for t in b.tokens if t['alive'] and t['player']==b.owner and t['name']==p['token']['name']);oldlife=b.life[b.owner]
 variable_token_combat(b,p,'ready_attack' if mode=='lifelink' else mode)
 if (mode in ('ready_attack','lifelink') or mode.startswith('block_')) and 'Lifelink' in chosen['keywords']:
  if mode in ('ready_attack','lifelink') or mode=='block_ground' or (mode=='block_flying' and bool({'Flying','Reach'}&set(chosen['keywords']))):b.life[b.owner]=oldlife+chosen['power']


def expand_wave6(name,csvid,d,p,selected,rows,oracle):
 if any(v.rsplit('_',1)[0] in ('countered','lost_target') for v in selected):
  out=[]
  for v in selected:out.extend(expand_wave6_resolution(name,csvid,d,p,[v],rows,oracle) if v.rsplit('_',1)[0] in ('countered','lost_target') else expand_wave6(name,csvid,d,p,[v],rows,oracle))
  return out
 if p['mechanism']=='damage_creator':return expand_wave6_damage(name,csvid,d,p,selected,rows,oracle)
 if p['mechanism']=='wolf_creator':return expand_wave6_wolves(name,csvid,d,p,selected,rows,oracle)
 if p['mechanism'] in ('land_creator','cost_creator'):return expand_wave6_activations(name,csvid,d,p,selected,rows,oracle)
 if p['mechanism'] in ('spell_count','spell_x','fixed_followup'):return expand_wave6_breadth(name,csvid,d,p,selected,rows,oracle)
 if p['mechanism'] in ('cast_predicate','tap_optional'):return expand_wave6_triggers(name,csvid,d,p,selected,rows,oracle)
 from token_trigger_creation_spec import wave6_data,wave6_variants,validate_wave6_witnesses,wave6_cost
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_variants(p) for v in selected):raise ValueError('unique supported Wave6 variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','effect_snapshot':'create','activation_snapshot':'create','flashback_snapshot':'flashback'}.get(variant,variant);kind=p['mechanism'];grave=mode.startswith('flashback');b=(Wave6SpellLedger if kind=='x_wrath' and mode.startswith('block_') else Wave6TokenLedger)(owner,oracle,rows,p,data);b.add(name,'source',zone='graveyard' if grave else 'hand');b.universe.add(p['token']['name']);b.add('Grizzly Bears','ordinary',owner,'battlefield')
  for player,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  if grave:b.emit(_variable_step('prepare_graveyard_abilities'),_variable_step('present_ai_dialogs',value=1))
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps);b.event_cut=ready_cut
  spell=kind in ('flashback_create','x_create','x_wrath','death_history');x=0 if mode=='zero' else 6 if mode=='larger_x' else 4 if mode=='below_threshold' else 5 if kind=='x_wrath' else 3
  if kind=='x_wrath':
   b.add('Ashcoat Bear','opposing_creature',1-owner,'battlefield');b.add('Forest','surviving_land',owner,'battlefield',tapped=True)
   if mode=='threshold_ownership':b.add('Suntail Hawk','foreign_creature',owner,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing_creature',1-owner,'battlefield',opponent_owned=True)
   b.witness_tokens('Raise the Alarm','independent_before_wrath',1-owner)
  if kind=='death_history':
   b.emit(_variable_step('reset_death_history'))
   if mode=='ownership':
    b.add('Ashcoat Bear','foreign_death',owner,'battlefield',opponent_owned=True);b.add('Suntail Hawk','owned_opposing_death',1-owner,'battlefield',opponent_owned=True);b.destroy('foreign_death');b.destroy('owned_opposing_death')
   elif mode!='zero':
    side=1-owner if mode=='opponent_deaths' else owner;b.add('Ashcoat Bear','death_fodder',side,'battlefield');b.destroy('death_fodder')
   if mode=='token_deaths':
    b.witness_tokens('Raise the Alarm','history_tokens');token=b.tokens[0];b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='dead_token'),_variable_step('destroy',ref='dead_token'),_variable_step('refresh'));b.drop_token(token)
   if mode=='prior_turn_expired':b.emit(_variable_step('advance_turn'));b.serial+=1;b.turn=1-owner
   if mode=='purged_history':b.purge()
  if kind=='servo_lord' and mode.startswith('static_'):
   b.witness_tokens('Servo Exhibition','independent_servos');b.witness_tokens('Ghirapur Gearcrafter','independent_thopter');b.witness_tokens('Servo Exhibition','opposing_servos',1-owner);b.witness_tokens('Ghirapur Gearcrafter','opposing_thopter',1-owner);b.proof_tokens()
  denied=mode in ('insufficient_mana','flashback_denied')
  if spell:
   if mode in ('payment','flashback_payment') or denied:b.pool(owner,[0]*6 if denied else [7,0,0,0,0,2] if grave else wave6_cost(oracle[name],x))
   b.event_cut=len(b.steps)
   if denied:
    if grave:b.emit(_variable_step('require_can_cast_graveyard',ref='source',value=0))
    b.emit(_variable_step('cast_graveyard' if grave else 'cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
   else:b.cast_creator(x,grave)
  else:
   if kind=='land_activation':b.add('Walking Atlas','land_atlas',owner,'battlefield');b.atlas('land_atlas','source')
   else:b.cast_permanent()
   b.event_cut=len(b.steps)
   if kind in ('land_activation','servo_lord'):
    if mode=='mana_ability':
     b.pool(owner,[0]*6);b.emit(_variable_step('require_can_activate',ref='source',value=1),_variable_step('activate',ref='source',value=1),_variable_step('assert_stack',value=0),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.entities['source']['tapped']=True;b.mana[owner][0]+=1
    elif denied:
     b.pool(owner,[0]*6)
     # The land can still activate its printed mana ability; don't mislabel that
     # availability query as the unavailable token-making choice.
     if kind=='servo_lord':b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0))
    else:
     if mode=='payment':b.pool(owner,p['activation']['mana'])
     b.activate_creator()
     if mode=='tapped_denied':b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0))
     if mode=='repeat':
      if p['activation']['tap']:b.pool(0,[0]*6);b.pool(1,[0]*6);b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('untap_step',player=owner));b.entities['source']['tapped']=False;b.entities['land_atlas']['tapped']=False;b.emit(_variable_step('turn',player=owner,value=0));b.pool(owner,[30,10,10,10,10,10])
      b.activate_creator()
   if kind=='artifact_cast':
    if mode=='source_removed':b.destroy('source')
    witness='Grizzly Bears' if mode=='nonartifact' else 'Bone Saw';player=1-owner if mode=='opponent_cast' else owner
    if mode=='no_payment':b.pool(owner,[0]*6)
    b.artifact_cast(witness,'cast_witness',player,accept=mode!='no_payment')
    if mode=='repeat':b.artifact_cast('Ornithopter','second_artifact')
   if kind=='tap_trigger':
    if mode=='source_removed':b.destroy('source');b.tap_spell('ordinary')
    elif mode=='source_attack':
     b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('turn',player=owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'));b.entities['source']['tapped']=True;b.create(p['token'],1);b.emit(_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.life[1-owner]-=p['power']
    elif mode=='untap_only':b.tap_spell(tap=False)
    elif mode=='other_tapped':b.tap_spell('ordinary')
    else:
     b.tap_spell()
     if mode=='repeat':b.tap_spell(tap=False);b.tap_spell()
  b.proof_tokens() # positive effect and live static bonus before source/token loss
  if mode=='static_removed':b.destroy('source');b.proof_tokens()
  if mode=='source_removed' and kind not in ('artifact_cast','tap_trigger'):b.destroy('source')
  live=[t for t in b.tokens if t['alive'] and t['player']==owner and t['name']==p['token']['name']]
  if live and mode in ('fresh_attack','ready_attack','block_ground','block_flying','lifelink'):
   if kind=='x_wrath' and mode.startswith('block_'):wave6_spell_combat(b,p,mode)
   else:wave6_combat(b,p,mode)
  if live and mode in ('destroy_token','bounce_token','exile_token'):
   token=live[0];b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='removed_token'))
   if mode=='destroy_token':b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
   else:
    b.spell('Unsummon' if mode=='bounce_token' else 'Swords to Plowshares','remove_token','removed_token')
    if mode=='exile_token':b.life[owner]+=token['power']
   b.drop_token(token,dies=mode=='destroy_token')
  if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else b.activation_cut if variant=='activation_snapshot' else b.event_cut
   if cut is None:raise ValueError('requested snapshot has no real boundary')
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['token doubling and replacement effects; source control changes after event','multiple blockers and damage assignment choices','separate trigger/activation priority responses except actual cast pending snapshots','upkeep/draw boundaries are not manufactured']
  if kind=='artifact_cast':gaps+=['optional payment decline with available mana; no-payment control has no remaining mana','countered parent cast history cannot be manufactured by the current early response helper']
  if kind=='land_activation':gaps+=['mana ability and token creation are separate modes; insufficient token mana does not deny the mana ability']
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out

# Wave6 count/X/followup ledger.
class Wave6SpellLedger(Wave6TokenLedger):
 def __init__(self,owner,oracle,rows,p,data):
  super().__init__(owner,oracle,rows,p,data);self.unordered_library=set();self.shuffle_count=[0,0]
 def add(self,name,ref,player=None,zone='hand',opponent_owned=False,tapped=False):
  if ref=='combat_witness' and ref in self.entities:
   assert self.entities[ref]['name']==name and self.entities[ref]['zone']=='battlefield'
   return ref
  return super().add(name,ref,player,zone,opponent_owned,tapped)
 def basis_amount(self,p):
  basis=p['amount']['basis'];own=self.owner
  if basis=='controlled_elf_permanents':return sum(e['zone']=='battlefield' and e['player']==own and 'Elf' in self.oracle[e['name']]['type_line'].split() for e in self.entities.values())+sum(t['alive'] and t['player']==own and 'Elf' in t['subtypes'] for t in self.tokens)
  if basis=='controlled_forests':return sum(e['zone']=='battlefield' and e['player']==own and 'Forest' in self.oracle[e['name']]['type_line'].split() for e in self.entities.values())
  if basis=='all_creatures':return sum(e['zone']=='battlefield' and e['creature'] for e in self.entities.values())+sum(t['alive'] for t in self.tokens)
  if basis=='greatest_controlled_creature_power':return max([0]+[e['power'] for e in self.entities.values() if e['zone']=='battlefield' and e['player']==own and e['creature']]+[t['power']+t['counters'] for t in self.tokens if t['alive'] and t['player']==own])
  raise ValueError('unsupported breadth count basis')
 def cast_breadth(self,p,x):
  from token_trigger_creation_spec import wave6_cost
  owner=self.owner;record=self.oracle[p['source_name']]
  # The native selector asks how much variable generic mana to pay. With XX
  # that payment is 2*Oracle X; the card's ordinary casting callback derives X.
  if '{X}' in record['mana_cost']:self.emit(_variable_step('choose_x',player=owner,value=x if p['source_name']=='Decree of Justice' else x*record['mana_cost'].count('{X}')))
  self.emit(_variable_step('cast',ref='source',value=1));self.pending_cut=len(self.steps);self.pay(owner,wave6_cost(record,x));self.emit(_variable_step('assert_stack',value=1),_variable_step('assert_zone_multiplicity',player=owner,zone='hand',index=self.rows[p['source_name']]['csvid'],value=1));self.emit(*(_variable_step('assert_mana',player=owner,color=i,value=v) for i,v in enumerate(self.mana[owner])));self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'))
  amount=self.basis_amount(p) if p['mechanism']=='spell_count' else p['amount'].get('fixed') if p['mechanism']=='fixed_followup' else p['amount'].get('fixed_count') if p['amount'].get('fixed_count') is not None else x
  token=copy.deepcopy(p['token'])
  if p['amount'].get('body_x'):token['power']=token['toughness']=x
  self.create(token,amount)
  if token['toughness']==0:
   for t in self.tokens:
    if t['alive'] and t['origin']=='source':self.drop_token(t)
  shuffled=any(c['kind']=='shuffle_owner_library' for c in p['operative']);self.move('source','library' if shuffled else 'graveyard')
  if shuffled:self.unordered_library.add(owner);self.shuffle_count[owner]+=1
 def finish(self):
  start=len(self.steps);super().finish()
  self.steps[start:]=[s for s in self.steps[start:] if not(s['op']=='assert_zone_card' and s.get('zone')=='library' and s.get('player') in self.unordered_library)]
  self.emit(*(_variable_step('assert_shuffle_count',player=player,value=value) for player,value in enumerate(self.shuffle_count)))
 def token_mana(self,all_tokens=False):
  selected=[t for t in self.tokens if t['alive'] and t['origin']=='source'];selected=selected if all_tokens else selected[:1]
  for token in selected:
   self.emit(_variable_step('bind_permanent_copy',player=self.owner,value=self.rows[token['name']]['csvid'],index=0,ref='mana_token'),_variable_step('require_can_activate',ref='mana_token',value=1),_variable_step('activate',ref='mana_token',value=1),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'),_variable_observe('mana_token','in_play',0));self.drop_token(token);self.mana[self.owner][0]+=1


def wave6_spell_prepare(b,p,mode):
 basis=p['amount'].get('basis');own=b.owner;side=1-own if mode=='opponent_state' else own;n=0 if mode=='zero' else 4 if mode=='larger_amount' else 2
 if basis=='controlled_elf_permanents':
  for i in range(n):b.add('Elvish Mystic','elf_'+str(i),side,'battlefield',tapped=mode in ('payment','insufficient_mana','wrong_color'))
  if mode=='ownership':b.add('Elvish Mystic','foreign_elf',own,'battlefield',opponent_owned=True);b.add('Elvish Mystic','owned_opposing_elf',1-own,'battlefield',opponent_owned=True)
  if mode=='token_input':b.spell('Elvish Promenade','earlier_prom');b.create(p['token'],b.basis_amount(p),origin='independent')
 if basis=='controlled_forests':
  for i in range(n):b.add('Forest','forest_'+str(i),side,'battlefield',tapped=True)
  b.add('Plains','ordinary_land',own,'battlefield',tapped=True)
  if mode=='ownership':b.add('Forest','foreign_forest',own,'battlefield',opponent_owned=True);b.add('Forest','owned_opposing_forest',1-own,'battlefield',opponent_owned=True)
 if basis=='all_creatures' and mode!='zero':
  for i in range(n):b.add('Ashcoat Bear','count_creature_'+str(i),side,'battlefield')
  if mode=='ownership':b.add('Suntail Hawk','foreign_creature',own,'battlefield',opponent_owned=True);b.add('Ashcoat Bear','owned_opposing_creature',1-own,'battlefield',opponent_owned=True)
  if mode=='preexisting_tokens':b.witness_tokens('Raise the Alarm','earlier_tokens',1-own)
 if basis=='greatest_controlled_creature_power' and mode!='zero':
  b.add('Scaled Wurm' if mode in ('larger_amount','opponent_state') else 'Grizzly Bears','greatest',side,'battlefield')
  if mode=='ownership':b.add('Scaled Wurm','foreign_wurm',own,'battlefield',opponent_owned=True);b.add('Scaled Wurm','owned_opposing_wurm',1-own,'battlefield',opponent_owned=True)
  if mode=='real_power_boost':b.spell('Giant Growth','power_boost','greatest');b.entities['greatest']['power']+=3;b.entities['greatest']['toughness']+=3


def wave6_spell_combat(b,p,mode):
 # Actual late creature casting keeps a defender out of the source's count
 # window. Fixture BF insertion after modeled resolution would be dishonest.
 if mode.startswith('block_'):
  witness='Suntail Hawk' if mode=='block_flying' else 'Grizzly Bears'
  if 'combat_witness' not in b.entities:
   b.emit(_variable_step('turn',player=1-b.owner,value=0));b.spell(witness,'combat_witness',player=1-b.owner);b.move('combat_witness','battlefield');b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'));b.serial+=2
 if mode=='flying_evasion':
  if 'ground_blocker' not in b.entities:b.spell('Grizzly Bears','ground_blocker',player=1-b.owner);b.move('ground_blocker','battlefield')
  start=len(b.steps);variable_token_combat(b,p,'ready_attack');cut=next(i for i in range(start,len(b.steps)) if b.steps[i]['op']=='advance_combat');b.steps.insert(cut,_variable_step('block',ref='ground_blocker',other='test_token',value=0));return
 if mode=='fresh_attack' and 'Haste' in p['token']['keywords']:
  token=next(t for t in b.tokens if t['alive'] and t['player']==b.owner);b.emit(_variable_step('bind_permanent_copy',player=b.owner,value=b.rows[token['name']]['csvid'],index=0,ref='test_token'),_variable_step('turn',player=b.owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='test_token',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));token['tapped']=True;b.life[1-b.owner]-=token['power'];return
 variable_token_combat(b,p,mode)


def expand_wave6_breadth(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_breadth_variants,validate_wave6_witnesses,wave6_cost
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_breadth_variants(p) for v in selected):raise ValueError('unique supported breadth variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','effect_snapshot':'create','token_mana_snapshot':'token_mana','expiry_snapshot':'cleanup'}.get(variant,variant);b=Wave6SpellLedger(owner,oracle,rows,p,data);b.add(name,'source');b.universe.add(p['token']['name'])
  if mode!='zero':b.add('Grizzly Bears','ordinary',owner,'battlefield')
  for who,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(who)+'_'+str(i),who,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps);wave6_spell_prepare(b,p,mode);event_cut=len(b.steps);x=0 if mode=='zero' else 5 if mode=='larger_amount' else 3
  denied=mode in ('insufficient_mana','wrong_color')
  if mode=='payment' or denied:
   cost=wave6_cost(oracle[name],x);pool=list(cost)
   if mode=='insufficient_mana':pool=[0]*6
   if mode=='wrong_color':
    color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
   b.pool(owner,pool)
  if denied:b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  else:
   b.cast_breadth(p,x);b.proof_tokens()
   if mode=='amount_locked':
    basis=p['amount']['basis'];ref='forest_0' if basis=='controlled_forests' else 'elf_0' if basis=='controlled_elf_permanents' else 'greatest' if basis=='greatest_controlled_creature_power' else 'count_creature_0';b.destroy(ref)
   if mode in ('fresh_attack','ready_attack','block_ground','block_flying','flying_evasion'):wave6_spell_combat(b,p,mode)
   if mode in ('destroy_token','bounce_token','exile_token'):
    token=next(t for t in b.tokens if t['alive'] and t['player']==owner);b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='removed_token'))
    if mode=='destroy_token':b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
    else:
     b.spell('Unsummon' if mode=='bounce_token' else 'Swords to Plowshares','token_removal','removed_token')
     if mode=='exile_token':b.life[owner]+=token['power']
    b.drop_token(token,dies=mode=='destroy_token')
   if mode.startswith('token_mana'):mana_cut=len(b.steps);b.token_mana(mode=='token_mana_all')
   if mode in ('cleanup','expiry','opposing_turn_expiry'):
    if mode=='opposing_turn_expiry':b.emit(_variable_step('turn',player=1-owner,value=0))
    expiry_cut=len(b.steps);b.emit(_variable_step('cleanup'),_variable_step('refresh'))
    delayed=next((c['kind'] for c in p['operative'] if c['kind'].endswith('next_end_step')),None)
    if delayed:
     for token in b.tokens:
      if token['alive'] and token['origin']=='source':b.drop_token(token,dies=delayed.startswith('sacrifice'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else mana_cut if variant=='token_mana_snapshot' else expiry_cut if variant=='expiry_snapshot' else event_cut
   if cut is None:raise ValueError('breadth snapshot has no real boundary')
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['token doubling/replacement effects and multiplayer','multiple blockers and damage assignment choices','source-spell pending response changes are not manufactured by early cast-history helpers']+['inactive: '+line for line in p['inactive_clauses']]
  if b.unordered_library:gaps+=['actual owner shuffle-call count and exact owner-library multiset conservation are asserted; shuffle distribution is unspecified']
  out.append(dict(name='TokenSpellExtensionRecipe_'+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out

# Wave6 predicate/tap creators.
import re
class Wave6TriggerLedger(Wave6SpellLedger):
 def event_spell(self,name,ref,player=None,accept=True,target=None):
  player=self.owner if player is None else player;rule=self.profile['operative'][0];record=self.oracle[name];type_line=record['type_line'];pred=rule['predicate'];symbols=set(re.findall(r'\{([BUGRW])\}',record['mana_cost']));matches={'artifact':'Artifact' in type_line,'noncreature':'Creature' not in type_line,'elf':'Elf' in type_line.split(),'enchantment':'Enchantment' in type_line,'spirit_or_arcane':bool({'Spirit','Arcane'}&set(type_line.split())),'multicolor':len(symbols)>=2}[pred];trigger=matches and player==self.owner and self.entities['source']['zone']=='battlefield';will_pay=trigger and accept
  self.add(name,ref,player)
  if name in ("Eyeblight's Ending",'Terminate'):target='ordinary'
  if target:self.emit(_variable_step('choose_target',player=player,ref=target))
  if will_pay and rule['optional'] and self.owner==0 and self.profile['source_name']!='Goblinslide':self.emit(_variable_step('choose_priority',ref='source'))
  if self.profile['source_name']=='Goblinslide' and not accept and 'optional_payment_land' in self.entities:self.emit(_variable_step('choose_cancel_mana',player=0))
  self.emit(_variable_step('cast',ref=ref,value=1));self.pending_cut=len(self.steps);self.emit(_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(player,variable_cost(record));self.move(ref,'battlefield' if any(t in type_line for t in ('Creature','Artifact','Enchantment')) else 'graveyard')
  if name in ("Eyeblight's Ending",'Terminate'):self.move('ordinary','graveyard')
  if name=='Reach Through Mists':
   drawn=self.zones['library'][player].pop(0);self.zones['hand'][player].append(drawn)
   for e in self.entities.values():
    if e['zone']=='library' and e['player']==player and e['name']==drawn:e['zone']='hand';break
  if will_pay:
   if rule['payment'] is not None:self.pay(self.owner,rule['payment'])
   self.create(self.profile['token'],1)
 def tap_optional(self,target='source',tap=True,accept=True):
  ref='tap_spell_'+str(sum(e['name']=='Twiddle' for e in self.entities.values()));self.add('Twiddle',ref,0);will_trigger=target=='source' and self.entities['source']['zone']=='battlefield' and tap and not self.entities['source']['tapped'];self.emit(_variable_step('choose_target',player=0,ref=target),_variable_step('choose_dialog',player=0,value=0 if tap else 1))
  if will_trigger and self.owner==0:self.emit(_variable_step('choose_dialog',player=0,value=0 if accept else 1))
  self.emit(_variable_step('cast',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(0,variable_cost(self.oracle['Twiddle']));self.move(ref,'graveyard');self.entities[target]['tapped']=tap
  if will_trigger and accept:self.create(self.profile['token'],1)


def wave6_trigger_witness(p,mode):
 pred=p['operative'][0]['predicate']
 first={'artifact':'Bone Saw','noncreature':'Bone Saw','elf':'Elvish Mystic','enchantment':'Seal of Cleansing','spirit_or_arcane':'Kami of Ancient Law','multicolor':'Watchwolf'}
 second={'artifact':'Ornithopter','noncreature':'Seal of Cleansing','elf':"Eyeblight's Ending",'enchantment':'Seal of Cleansing','spirit_or_arcane':'Reach Through Mists','multicolor':'Terminate'}
 wrong={'artifact':'Grizzly Bears','noncreature':'Grizzly Bears','elf':'Grizzly Bears','enchantment':'Bone Saw','spirit_or_arcane':'Elvish Mystic','multicolor':'Grizzly Bears'}
 return wrong[pred] if mode=='nonmatching' else second[pred] if mode=='predicate_second_type' else first[pred]


def expand_wave6_triggers(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_trigger_variants,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_trigger_variants(p) for v in selected):raise ValueError('unique supported trigger variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','pending_snapshot':'create','event_snapshot':'create'}.get(variant,variant);b=Wave6TriggerLedger(owner,oracle,rows,p,data);b.add(name,'source');b.add('Grizzly Bears','ordinary',owner,'battlefield');b.universe.add(p['token']['name'])
  for who,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(who)+'_'+str(i),who,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps);b.cast_permanent();event_cut=len(b.steps)
  if mode=='source_removed':b.destroy('source')
  if p['mechanism']=='cast_predicate':
   witness=wave6_trigger_witness(p,mode);player=1-owner if mode=='opponent_event' else owner
   if name=='Goblinslide' and mode=='decline':b.add('Forest','optional_payment_land',owner,'battlefield');b.pool(owner,[0]*6)
   if mode in ('no_payment','exact_payment'):
    c=variable_cost(oracle[witness]);pool=[x+y for x,y in zip(c,p['operative'][0]['payment'])] if mode=='exact_payment' else c;b.pool(owner,pool)
   b.event_spell(witness,'event_spell',player,accept=mode not in ('decline','no_payment'))
   if mode=='repeat':b.event_spell(witness,'repeated_spell')
  else:
   if mode=='opponent_event':b.add('Ashcoat Bear','opposing_tap',1-owner,'battlefield');b.tap_optional('opposing_tap')
   elif mode in ('nonmatching','source_removed'):b.tap_optional('ordinary')
   elif mode=='untap_only':b.tap_optional(tap=False)
   elif mode=='source_attack':
    b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('turn',player=owner,value=0),_variable_step('begin_attackers'))
    if owner==0:b.emit(_variable_step('choose_dialog',player=owner,value=0))
    b.emit(_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.entities['source']['tapped']=True;b.create(p['token'],1);b.life[1-owner]-=p['power']
   else:
    b.tap_optional(accept=mode!='decline')
    if mode=='repeat':b.tap_optional(tap=False);b.tap_optional()
  b.proof_tokens()
  if mode in ('fresh_attack','ready_attack','block_ground','block_flying','flying_evasion'):wave6_spell_combat(b,p,mode)
  if mode=='source_flying_evasion':
   b.spell('Grizzly Bears','ground_blocker',player=1-owner);b.move('ground_blocker','battlefield');b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('turn',player=owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('block',ref='ground_blocker',other='source',value=0),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.entities['source']['tapped']=True;b.life[1-owner]-=p['power']
  if mode in ('destroy_token','bounce_token','exile_token'):
   token=next(t for t in b.tokens if t['alive'] and t['origin']=='source');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='removed_token'))
   if mode=='destroy_token':b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
   else:
    removal='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares'
    if p['mechanism']=='cast_predicate':b.event_spell(removal,'token_removal',target='removed_token')
    else:b.spell(removal,'token_removal','removed_token')
    if mode=='exile_token':b.life[owner]+=token['power']
   b.drop_token(token,dies=mode=='destroy_token')
  if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='pending_snapshot' else event_cut
   if cut is None:raise ValueError('trigger snapshot has no actual boundary')
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['separate priority responses to committed cast triggers; no early-history substitute','multiple source copies, replacement effects and multiplayer','optional AI-decline policy is not forced into a nonexistent choice']+['inactive: '+line for line in p['inactive_clauses']]
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out

# Wave6 land/additional-cost ledger.
class Wave6ActivationLedger(Wave6SpellLedger):
 def has_effect(self,kind):return any(c['kind']==kind for c in self.profile['operative'])
 def active_source(self):return self.entities.get('source',{}).get('zone')=='battlefield'
 def create(self,definition,amount,player=None,origin='source'):
  start=len(self.tokens);super().create(definition,amount,player,origin)
  for token in self.tokens[start:]:
   if origin=='source' and self.has_effect('tokens_enter_tapped'):token['tapped']=True
   if self.active_source() and self.has_effect('another_controlled_spirit_entry_gain2') and token['player']==self.owner and 'Spirit' in token['subtypes']:self.life[self.owner]+=2
 def move(self,ref,zone):
  e=self.entities[ref];enter=zone=='battlefield' and e['zone']!='battlefield' and ref!='source' and e['player']==self.owner and 'Spirit' in self.oracle[e['name']]['type_line'].split();super().move(ref,zone)
  if enter and self.active_source() and self.has_effect('another_controlled_spirit_entry_gain2'):self.life[self.owner]+=2
 def drop_token(self,token,dies=True):
  drain=dies and token['alive'] and token['player']==self.owner and 'Saproling' in token['subtypes'] and self.has_effect('controlled_saproling_death_drain') and (self.active_source() or getattr(self,'same_event_lki',False));super().drop_token(token,dies)
  if drain:self.life[self.owner]+=1;self.life[1-self.owner]-=1
 def actual_untap(self):
  NoncreatureTokenLedger.actual_untap(self);self.emit(_variable_step('turn',player=self.owner,value=0));self.pool(0,[30,10,10,10,10,10]);self.pool(1,[30,10,10,10,10,10])
 def land_entry(self):
  self.add('Walking Atlas','land_atlas',self.owner,'battlefield');self.emit(_variable_step('require_can_activate',ref='land_atlas',value=1),_variable_step('activate',ref='land_atlas',value=1),_variable_step('assert_stack',value=1),_variable_step('choose_zone_card',player=self.owner,value=self.rows[self.profile['source_name']]['csvid']),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.entities['land_atlas']['tapped']=True;self.move('source','battlefield');self.entities['source']['tapped']=self.has_effect('source_enters_tapped');self.emit(_variable_observe('source','in_play',1),_variable_observe('source','tapped',int(self.entities['source']['tapped'])))
  entry=next((c['count'] for c in self.profile['operative'] if c['kind']=='entry_creation'),0)
  if entry:self.create(self.profile['token'],entry)
 def tap_spell(self,target='source',tap=True):
  ref='tap_spell_'+str(sum(e['name']=='Twiddle' for e in self.entities.values()));self.add('Twiddle',ref,0);self.emit(_variable_step('choose_target',player=0,ref=target),_variable_step('choose_dialog',player=0,value=0 if tap else 1),_variable_step('cast',ref=ref,value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(0,variable_cost(self.oracle['Twiddle']));self.move(ref,'graveyard');self.entities[target]['tapped']=tap
 def cost_inputs(self,mode):
  a=self.profile['activation'];kind=a and a['additional'];suffix='_repeat' if mode=='repeat' else '';own=self.owner;side=1-own if mode=='opponent_cost' else own;refs=[]
  if kind=='exile_own_grave_creature':
   self.add('Forest','noncreature_grave'+suffix,own,'graveyard')
   if mode!='missing_cost':refs=[self.add('Ashcoat Bear','grave_cost'+suffix,side,'graveyard')]
   self.add('Grizzly Bears','unselected_grave'+suffix,1-own,'graveyard')
  if kind=='sacrifice_land' and mode!='missing_cost':refs=[self.add('Forest','land_cost'+suffix,side,'battlefield',opponent_owned=mode=='foreign_cost')]
  if kind=='sacrifice_goblin':
   count=1 if mode=='missing_cost' else 2
   for i in range(count):refs.append(self.add('Goblin Piker' if i==0 else 'Mogg Fanatic','goblin_cost_'+str(i)+suffix,side,'battlefield',opponent_owned=mode=='foreign_cost' and i==1))
  if kind=='tap_other_creature' and mode!='missing_cost':
   if mode=='fresh_other_cost':self.spell('Ashcoat Bear','tap_cost'+suffix);self.move('tap_cost'+suffix,'battlefield');refs=['tap_cost'+suffix]
   else:refs=[self.add('Ashcoat Bear','tap_cost'+suffix,side,'battlefield',opponent_owned=mode=='foreign_cost')]
  if kind=='tap_elf':
   refs=['source'];count=3 if mode=='missing_cost' else 4
   for i in range(count):
    ref='elf_cost_'+str(i)+suffix
    if mode=='fresh_other_cost':self.spell('Elvish Mystic',ref);self.move(ref,'battlefield');refs.append(ref)
    else:refs.append(self.add('Elvish Mystic',ref,side,'battlefield',opponent_owned=mode=='foreign_cost' and i==count-1))
  return refs
 def activation(self,mode,refs):
  a=self.profile['activation'];own=self.owner;self.activation_cut=len(self.steps)
  if a['additional']=='exile_own_grave_creature':self.emit(_variable_step('choose_zone_card',player=own,value=self.rows[self.entities[refs[0]]['name']]['csvid']))
  else:
   for ref in refs:
    if not(own==1 and a['additional'] in ('sacrifice_land','sacrifice_goblin')):self.emit(_variable_step('choose_target',player=own,ref=ref))
  if self.profile['mechanism']=='land_creator' and (self.profile['source_name']!='Cradle of the Accursed' or own==0):self.emit(_variable_step('choose_dialog',player=own,value=1))
  self.emit(_variable_step('require_can_activate',ref='source',value=1),_variable_step('activate',ref='source',value=1));self.pay(own,a['mana'])
  if a['tap']:self.entities['source']['tapped']=True
  if a['untap']:self.entities['source']['tapped']=False
  if a['sacrifice']:self.move('source','graveyard')
  for ref in refs:
   if a['additional'] in ('tap_elf','tap_other_creature'):self.entities[ref]['tapped']=True
   else:self.move(ref,'exile' if a['additional']=='exile_own_grave_creature' else 'graveyard')
  self.emit(_variable_step('assert_stack',value=1));self.emit(*(_variable_step('assert_mana',player=own,color=i,value=v) for i,v in enumerate(self.mana[own])))
  for ref in (['source'] if a['sacrifice'] else [])+refs:
   e=self.entities[ref]
   if e['initial_zone'] in ('hand','battlefield'):self.emit(_variable_observe(ref,'in_play',int(e['zone']=='battlefield')))
   if e['zone']=='battlefield':self.emit(_variable_observe(ref,'tapped',int(e['tapped'])))
   else:self.emit(_variable_step('assert_zone_multiplicity',player=e['player'],zone=e['zone'],index=self.rows[e['name']]['csvid'],value=self.zones[e['zone']][e['player']].count(e['name'])))
  if mode=='source_removed_pending':self.destroy('source')
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.create(self.profile['token'],self.profile['amount']['fixed'])
 def draw_one(self):
  own=self.owner;name=self.zones['library'][own].pop(0);self.zones['hand'][own].append(name)
  for e in self.entities.values():
   if e['zone']=='library' and e['player']==own and e['name']==name:e['zone']='hand';break
 def lifegain_draw(self,amount,accept=True):
  self.life[self.owner]+=amount
  if self.active_source() and self.has_effect('own_lifegain_optional_pay2_draw1') and accept:self.pay(self.owner,[2,0,0,0,0,0]);self.draw_one()
 def lifegain_spell(self,target,accept=True):
  own=self.owner;e=self.entities[target];gain_player=e['owner'];gain=e['power'];trigger=self.active_source() and self.has_effect('own_lifegain_optional_pay2_draw1') and gain_player==own and accept;self.add('Swords to Plowshares','life_spell',own);self.emit(_variable_step('choose_target',player=own,ref=target))
  self.emit(_variable_step('cast',ref='life_spell',value=1),_variable_step('assert_stack',value=1))
  if trigger and own==0:self.emit(_variable_step('choose_priority',ref='source'))
  self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.pay(own,variable_cost(self.oracle['Swords to Plowshares']));self.move('life_spell','graveyard');self.move(target,'exile');self.life[gain_player]+=gain
  if trigger:self.pay(own,[2,0,0,0,0,0]);self.draw_one()
 def wrath_event(self):
  self.same_event_lki=self.active_source();self.spell('Wrath of God','wrath')
  for ref,e in self.entities.items():
   if e['zone']=='battlefield' and e['creature']:self.move(ref,'graveyard')
  self.kill_tokens();self.same_event_lki=False


def wave6_activation_combat(b,p,mode):
 token=next(t for t in b.tokens if t['alive'] and t['player']==b.owner and t['name']==p['token']['name']);own=b.owner
 if b.has_effect('tokens_enter_tapped') and mode in ('ready_attack','token_untap_attack'):
  b.actual_untap();mode='ready_attack'
 if b.has_effect('tokens_enter_tapped') and mode in ('block_ground','block_flying','tapped_token_block_denied'):
  if 'combat_witness' not in b.entities:raise ValueError('tapped-token denial requires a ready battlefield attacker')
  b.emit(_variable_step('bind_permanent_copy',player=own,value=b.rows[token['name']]['csvid'],index=0,ref='test_token'),_variable_step('turn',player=1-own,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='combat_witness',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('block',ref='test_token',other='combat_witness',value=0),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.entities['combat_witness']['tapped']=True;b.life[own]-=b.entities['combat_witness']['power'];return
 if mode=='trample_damage':
  b.spell('Grizzly Bears','combat_witness',player=1-own);b.move('combat_witness','battlefield');b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('bind_permanent_copy',player=own,value=b.rows[token['name']]['csvid'],index=0,ref='test_token'),_variable_step('turn',player=own,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='test_token',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('block',ref='combat_witness',other='test_token',value=1),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));token['tapped']=True;token['damage']=2;b.move('combat_witness','graveyard');b.life[1-own]-=token['power']-2;return
 start=len(b.steps);wave6_spell_combat(b,p,'ready_attack' if mode in ('lifelink_draw','lifelink_no_payment') else mode)
 if 'Lifelink' in token['keywords']:
  deals=mode in ('ready_attack','lifelink_draw','lifelink_no_payment','block_ground') or mode=='block_flying' and bool({'Flying','Reach'}&set(token['keywords']))
  if deals:
   draw=b.active_source() and b.has_effect('own_lifegain_optional_pay2_draw1') and mode!='lifelink_no_payment'
   if draw and own==0:
    pos=next(i for i in range(start,len(b.steps)) if b.steps[i]['op']=='combat_damage');b.steps.insert(pos,_variable_step('choose_priority',ref='source'))
   b.lifegain_draw(token['power'],accept=draw)


def expand_wave6_activations(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_activation_variants,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_activation_variants(p) for v in selected):raise ValueError('unique supported activation variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','event_snapshot':'create','activation_snapshot':'create','token_mana_snapshot':'token_mana','death_snapshot':'saproling_death'}.get(variant,variant);b=Wave6ActivationLedger(owner,oracle,rows,p,data);land=p['mechanism']=='land_creator';foreign=mode=='foreign_source';b.add(name,'source',zone='battlefield' if foreign else 'hand',opponent_owned=foreign,tapped=foreign and b.has_effect('source_enters_tapped'));b.add('Grizzly Bears','ordinary',1-owner if mode in ('missing_cost','opponent_cost') and p['activation'] and p['activation']['additional']=='tap_other_creature' else owner,'battlefield');b.universe.add(p['token']['name'])
  if mode in ('block_ground','block_flying','tapped_token_block_denied'):b.add('Suntail Hawk' if mode=='block_flying' else 'Grizzly Bears','combat_witness',1-owner,'battlefield')
  for who,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(who)+'_'+str(i),who,'library')
  b.emit(_variable_step('turn',player=owner,value=0),_variable_step('present_ai_dialogs',value=1));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps)
  if land:
   if not foreign:b.land_entry()
  else:b.cast_permanent()
  event_cut=len(b.steps);a=p['activation']
  if mode=='enchantment_target':b.add('Disenchant','type_probe');b.emit(_variable_step('require_can_cast',ref='type_probe',value=0))
  skip=mode in ('entry_tapped_denied','mana_ability')
  if mode=='entry_tapped_denied':b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0),_variable_step('assert_stack',value=0))
  else:
   if land and b.entities['source']['tapped']:
    b.proof_tokens()
    if not(name=='Khalni Garden' and mode=='fresh_attack'):b.actual_untap()
   if mode=='mana_ability':
    b.pool(owner,[0]*6)
    if owner==1 and name not in ('Khalni Garden','Cradle of the Accursed'):b.emit(_variable_step('choose_dialog',player=owner,value=0))
    b.emit(_variable_step('require_can_activate',ref='source',value=1),_variable_step('activate',ref='source',value=1),_variable_step('assert_stack',value=0),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.entities['source']['tapped']=True;color=next(c['color'] for c in p['operative'] if c['kind']=='tap_mana');b.mana[owner][0 if color=='C' else VARIABLE_COLOR_INDEX[color]]+=1
   if a and not skip:
    if p['creature'] and (a['tap'] or a['untap']) and mode!='fresh_denied':b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'))
    refs=b.cost_inputs('create' if mode=='repeat' else mode);repeat_refs=b.cost_inputs('repeat') if mode=='repeat' and a['additional'] in ('sacrifice_land','sacrifice_goblin','exile_own_grave_creature') and not(name=='Goblin Warrens' and owner==1) else None
    if a['untap'] and mode!='tapped_or_untapped_denied':b.tap_spell()
    if a['tap'] and mode=='tapped_or_untapped_denied':b.tap_spell()
    denied=mode in ('insufficient_mana','wrong_color','missing_cost','opponent_cost','fresh_denied','tapped_or_untapped_denied')
    if mode in ('payment','insufficient_mana','wrong_color'):
     pool=list(a['mana']) if mode!='insufficient_mana' else [0]*6
     if mode=='wrong_color':color=next(i for i in range(1,6) if pool[i]);pool[1 if color!=1 else 2]+=pool[color];pool[color]=0
     b.pool(owner,pool)
    if denied:b.emit(_variable_step('require_can_activate',ref='source',value=0),_variable_step('activate',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
    else:
     b.activation(mode,refs)
     if mode=='repeat':
      if a['tap'] or a['additional'] in ('tap_elf','tap_other_creature'):b.actual_untap()
      if a['untap']:b.tap_spell()
      if name=='Goblin Warrens' and owner==1:
       b.proof_tokens();b.emit(_variable_step('require_can_activate',ref='source',value=1),_variable_step('activate',ref='source',value=1),_variable_step('assert_stack',value=1));b.pay(owner,a['mana'])
       for token in [t for t in b.tokens if t['alive'] and t['player']==owner][:2]:b.drop_token(token)
       b.emit(*(_variable_step('assert_mana',player=owner,color=i,value=v) for i,v in enumerate(b.mana[owner])));b.proof_tokens();b.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.create(p['token'],p['amount']['fixed'])
      else:
       if repeat_refs is not None:refs=repeat_refs
       b.activation('repeat',refs)
     if mode=='foreign_cost':
      for ref in refs:
       e=b.entities[ref]
       if e['zone']=='battlefield' and e['owner']!=owner:b.destroy(ref)
  b.proof_tokens()
  live=[t for t in b.tokens if t['alive'] and t['player']==owner]
  if live and mode in ('fresh_attack','ready_attack','block_ground','block_flying','flying_evasion','trample_damage','token_untap_attack','tapped_token_block_denied','lifelink_draw','lifelink_no_payment'):
   if mode=='lifelink_no_payment':b.pool(owner,[0]*6)
   wave6_activation_combat(b,p,mode)
  if live and mode in ('destroy_token','bounce_token','exile_token','saproling_death','source_removed_death','death_snapshot'):
   token=live[0]
   if mode=='source_removed_death':b.destroy('source')
   b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='removed_token'));death_cut=len(b.steps)
   if mode in ('destroy_token','saproling_death','source_removed_death','death_snapshot'):b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
   else:
    spell='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares';b.add(spell,'token_removal');b.emit(_variable_step('choose_target',player=owner,ref='removed_token'));draw=mode=='exile_token' and b.active_source() and b.has_effect('own_lifegain_optional_pay2_draw1')
    if draw and owner==0:b.pool(owner,[2,0,0,0,0,1])
    b.emit(_variable_step('cast',ref='token_removal',value=1),_variable_step('assert_stack',value=1))
    if draw and owner==0:b.emit(_variable_step('choose_priority',ref='source'))
    b.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle[spell]));b.move('token_removal','graveyard')
    if mode=='exile_token':b.lifegain_draw(token['power'],accept=draw)
   b.drop_token(token,dies=mode not in ('bounce_token','exile_token'))
  if mode=='ordinary_death':b.destroy('ordinary')
  if mode=='opponent_saproling_death':
   b.emit(_variable_step('turn',player=1-owner,value=0));b.turn=1-owner;b.witness_tokens('Yavimaya Sapherd','opposing_saprolings',1-owner);token=next(t for t in b.tokens if t['alive'] and t['player']==1-owner);b.emit(_variable_step('bind_permanent_copy',player=1-owner,value=rows[token['name']]['csvid'],index=0,ref='opposing_sap'),_variable_step('destroy',ref='opposing_sap'),_variable_step('refresh'));b.drop_token(token)
  if mode=='wrath_death':b.wrath_event()
  if mode in ('normal_spirit_entry','opponent_spirit_entry','ordinary_entry','source_removed_entry'):
   if mode=='source_removed_entry':b.destroy('source')
   witness='Ashcoat Bear' if mode=='ordinary_entry' else 'Kami of Ancient Law';player=1-owner if mode=='opponent_spirit_entry' else owner;b.spell(witness,'entry_witness',player=player);b.move('entry_witness','battlefield')
  if mode in ('normal_lifegain_draw','opponent_lifegain','source_removed_lifegain'):
   if mode=='source_removed_lifegain':b.destroy('source')
   target='ordinary'
   if mode=='opponent_lifegain':target=b.add('Ashcoat Bear','opposing_lifegain',1-owner,'battlefield')
   if mode=='normal_lifegain_draw' and owner==0:b.pool(owner,[2,0,0,0,0,1])
   b.lifegain_spell(target)
  if mode=='source_flying_evasion':
   b.spell('Grizzly Bears','source_blocker',player=1-owner);b.move('source_blocker','battlefield');b.emit(_variable_step('advance_turn'),_variable_step('advance_turn'),_variable_step('turn',player=owner,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='source',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('block',ref='source_blocker',other='source',value=0),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));b.entities['source']['tapped']=True;b.life[1-owner]-=p['power']
  if mode=='enchantment_target':
   b.emit(_variable_step('turn',player=owner,value=0),_variable_step('assert_stack',value=0),_variable_step('require_can_cast',ref='type_probe',value=1))
   token=next(t for t in b.tokens if t['alive'] and t['player']==owner);b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='type_token'),_variable_step('choose_target',player=owner,ref='type_token'),_variable_step('cast',ref='type_probe',value=1),_variable_step('assert_stack',value=1),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle['Disenchant']));b.move('type_probe','graveyard');b.drop_token(token)
  if mode in ('token_mana','token_mana_snapshot'):mana_cut=len(b.steps);b.token_mana()
  if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.activation_cut if variant=='activation_snapshot' else mana_cut if variant=='token_mana_snapshot' else death_cut if variant=='death_snapshot' else event_cut
   if cut is None:raise ValueError('activation snapshot has no actual boundary')
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['multiple blockers and damage assignment choices','replacement effects, multiplayer and source controller changes after ability activation','activated-ability counterspells; source removal while pending is covered where source was not sacrificed']
  if land:gaps+=['land entry uses real Walking Atlas; normal land-play permission and alternate entry methods untested','token-mode-only availability cannot be inferred from a combined mana-ability query']
  if a and a['only_sorcery']:gaps+=['token ability exercised in legal own main phase with empty stack; unavailable alternate dialog mode has no separate permission observer']
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out

# Wave6 damage-trigger ledger; creator Oracle supplies predicates.
class Wave6DamageLedger(Wave6ActivationLedger):
 def finish(self):
  start=len(self.steps);super().finish()
  for action in self.steps[start:]:
   if action['op']=='assert_card' and action.get('property')=='plus1_counters' and action.get('ref') in self.entities:action['value']=self.entities[action['ref']].get('counters',0)
 def age(self):
  turns=2 if self.turn==self.owner else 1;self.emit(*(_variable_step('advance_turn') for _ in range(turns)));self.serial+=turns;self.turn=self.owner
  for e in self.entities.values():e['damage']=0
  for t in self.tokens:t['damage']=0
 def damage_token(self,amount,player=None):self.create(self.profile['token'],amount,player)
 def shooter(self,player=None):
  player=self.owner if player is None else player;self.emit(_variable_step('turn',player=player,value=0));self.turn=player;self.spell('Triskelion','shooter',player=player);self.move('shooter','battlefield');e=self.entities['shooter'];e['counters']=3;e['power']=e['toughness']=4
  self.emit(_variable_observe('shooter','plus1_counters',3),_variable_observe('shooter','power',4),_variable_observe('shooter','toughness',4),_variable_step('assert_matching_permanents',player=player,index=self.rows['Triskelion']['csvid'],property='artifact',value=1),_variable_step('assert_matching_permanents',player=player,index=self.rows['Triskelion']['csvid'],property='token',value=0))
 def shoot(self,target=None,target_player=None,sentinel=False):
  self.damage_cut=len(self.steps);own=self.owner;player=self.entities['shooter']['player'];source_alive=self.active_source();pred=self.profile['operative'][0]['predicate'];self.emit(_variable_step('choose_player',player=player,value=target_player) if target_player is not None else _variable_step('choose_target',player=player,ref=target))
  if sentinel and own==0:self.emit(_variable_step('choose_trigger',ref='source'))
  self.emit(_variable_step('require_can_activate',ref='shooter',value=1),_variable_step('activate',ref='shooter',value=1),_variable_step('assert_stack',value=1));e=self.entities['shooter'];e['counters']-=1;e['power']-=1;e['toughness']-=1;self.emit(_variable_observe('shooter','plus1_counters',e['counters']),_variable_step('resolve'),_variable_step('refresh'))
  if sentinel and own==0:self.emit(_variable_step('assert_unused_choices',value=1))
  self.emit(_variable_step('assert_choices_consumed'))
  if target_player is not None:self.life[target_player]-=1
  else:
   e=self.entities[target];e['damage']+=1
   if e['damage']>=e['toughness']:self.move(target,'graveyard')
  if pred=='source_dealt_damage' and source_alive and target=='source':self.damage_token(1)
 def attack_event(self,attacker='source',blocker=None,can_block=True,can_attack=True,token=None,accept=True):
  own=self.owner;source_alive=self.active_source();rule=self.profile['operative'][0];pred=rule['predicate'];e=token or self.entities[attacker];player=e['player'];power=e['power']+(e.get('counters',0) if token else 0);toughness=e['toughness']+(e.get('counters',0) if token else 0);keywords=e['keywords'] if token else self.profile['source_keywords'] if attacker=='source' else self.oracle[e['name']]['keywords'];subtypes=e['subtypes'] if token else self.oracle[e['name']]['type_line'].split();artifact=e['artifact'] if token else 'Artifact' in self.oracle[e['name']]['type_line'];blocks=bool(blocker and can_block and can_attack);incoming=0;player_damage=power if can_attack and not blocks else 0;blocker_damage=0
  if blocks:
   be=self.entities[blocker];blocker_damage=min(power,be['toughness']) if 'Trample' in keywords else power;player_damage=max(0,power-be['toughness']) if 'Trample' in keywords else 0;incoming=0 if 'First strike' in keywords and power>=be['toughness'] else be['power']
  qualifies=source_alive and player_damage>0 and (pred=='self_combat_player' and attacker=='source' or pred=='controlled_artifact_creature_combat_player' and player==own and artifact or pred=='global_sliver_combat_player' and 'Sliver' in subtypes)
  if player!=self.turn:
   self.emit(_variable_step('advance_turn'));self.turn=player;self.serial+=1
   for entity in self.entities.values():entity['damage']=0
   for old_token in self.tokens:old_token['damage']=0
  self.emit(_variable_step('turn',player=player,value=0),_variable_step('begin_attackers'),_variable_step('attack',ref=attacker,value=int(can_attack)),_variable_step('finish_attackers'),_variable_step('begin_blockers'))
  if blocker:self.emit(_variable_step('block',ref=blocker,other=attacker,value=int(blocks)))
  self.emit(_variable_step('advance_combat'))
  self.damage_cut=len(self.steps)
  if qualifies and accept and rule['optional'] and pred!='global_sliver_combat_player' and own==0:self.emit(_variable_step('choose_priority',ref='source'))
  self.emit(_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'))
  if can_attack:e['tapped']='Vigilance' not in keywords;self.life[1-player]-=player_damage
  if blocks:
   be=self.entities[blocker]
   if blocker_damage>=be['toughness']:self.move(blocker,'graveyard')
   else:be['damage']=blocker_damage
   if incoming>=toughness:
    if token:self.drop_token(token)
    else:self.move(attacker,'graveyard')
   else:e['damage']=incoming
  if qualifies and accept:self.damage_token(player_damage if self.profile['amount']['basis']=='damage' else 1,player if pred=='global_sliver_combat_player' else own)
  if pred=='source_dealt_damage' and source_alive and attacker=='source' and incoming>0:self.damage_token(1)


def expand_wave6_damage(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_damage_variants,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_damage_variants(p) for v in selected):raise ValueError('unique supported damage variants required')
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','event_snapshot':'create','damage_snapshot':'create','token_mana_snapshot':'token_mana'}.get(variant,variant);b=Wave6DamageLedger(owner,oracle,rows,p,data);b.add(name,'source');b.add('Grizzly Bears','ordinary',owner,'battlefield');b.universe.add(p['token']['name']);pred=p['operative'][0]['predicate'];other=pred!='source_dealt_damage' and mode in ('other_creature','opponent_creature','artifact_creature','nonartifact_creature','own_sliver','opponent_sliver','foreign_sliver');attacker='source';player=owner
  if other:
   player=1-owner if mode in ('opponent_creature','opponent_sliver') else owner;witness='Sliver Construct' if mode in ('own_sliver','opponent_sliver','foreign_sliver') else 'Cathodion' if mode=='artifact_creature' or pred=='controlled_artifact_creature_combat_player' and mode=='opponent_creature' else 'Ashcoat Bear';attacker=b.add(witness,'other_attacker',player,'battlefield',opponent_owned=mode=='foreign_sliver')
  if mode=='source_removed':b.add('Sliver Construct' if pred=='global_sliver_combat_player' else 'Cathodion' if pred=='controlled_artifact_creature_combat_player' else 'Ashcoat Bear','followup_attacker',owner,'battlefield')
  blocker=None;can_block=True
  if mode in ('blocked','blocked_zero','flying_evasion'):
   witness='Suntail Hawk' if 'Flying' in p['source_keywords'] and mode=='blocked' else 'Scaled Wurm' if mode=='blocked_zero' and p['power']>2 else 'Grizzly Bears';blocker=b.add(witness,'blocker',1-owner,'battlefield');can_block=mode!='flying_evasion'
  for who,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(who)+'_'+str(i),who,'library')
  b.emit(_variable_step('turn',player=owner,value=0));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps);b.cast_permanent();event_cut=len(b.steps)
  if pred=='source_dealt_damage' and mode!='blocked':
   if mode=='lethal_damage':
    b.spell('Lightning Bolt','lethal_bolt','source');b.move('source','graveyard');b.damage_token(1);b.damage_cut=event_cut
   else:
    b.shooter(1-owner if mode=='opponent_creature' else owner);b.shoot('ordinary' if mode=='other_creature' else 'source')
    if mode=='multiple_damage_events':b.proof_tokens();b.shoot('source')
  elif mode=='noncombat_damage':b.shooter();b.shoot(target_player=1-owner,sentinel=True)
  else:
   if mode!='fresh_denied':b.age()
   b.attack_event(attacker,blocker,can_block,can_attack=mode!='fresh_denied',accept=mode!='decline')
  b.proof_tokens()
  if mode=='foreign_sliver':b.destroy('other_attacker')
  if mode=='source_removed':
   if b.active_source():b.destroy('source')
   b.age();b.attack_event('followup_attacker')
  if mode=='token_attack':
   token=next(t for t in b.tokens if t['alive'] and t['player']==owner);b.age();b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='token_attacker'));ground=None
   if 'Flying' in token['keywords']:ground=b.add('Grizzly Bears','token_ground_blocker',1-owner,'battlefield')
   b.attack_event('token_attacker',ground,can_block=False,token=token)
  if mode in ('destroy_token','bounce_token','exile_token'):
   token=next(t for t in b.tokens if t['alive'] and t['player']==owner);b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows[token['name']]['csvid'],index=0,ref='removed_token'))
   if mode=='destroy_token':b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
   else:
    b.spell('Unsummon' if mode=='bounce_token' else 'Swords to Plowshares','token_removal','removed_token')
    if mode=='exile_token':b.life[owner]+=token['power']
   b.drop_token(token,dies=mode=='destroy_token')
  if mode in ('token_mana','token_mana_all','token_mana_snapshot'):mana_cut=len(b.steps);b.token_mana(mode=='token_mana_all')
  if mode=='cleanup':
   b.emit(_variable_step('cleanup'),_variable_step('refresh'))
   for e in b.entities.values():e['damage']=0
   for token in b.tokens:token['damage']=0
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.damage_cut if variant=='damage_snapshot' else mana_cut if variant=='token_mana_snapshot' else event_cut
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['multiple blockers and damage assignment choices','source control changes after damage and between trigger placement/resolution','planeswalker damage, multiplayer and independent replacement effects']+['inactive: '+line for line in p['inactive_clauses']]
  if pred=='global_sliver_combat_player':gaps+=['independent recipient-decline policy is not manufactured by a generic source choice; positive acceptance both controllers is exercised']
  out.append(dict(name='TokenTriggerCreationRecipe_'+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out

# Wave6 Wolf observer cohort with independent source/normal witness pins.
class Wave6WolfLedger(Wave6DamageLedger):
 def __init__(self,owner,oracle,rows,p,data):
  super().__init__(owner,oracle,rows,p,data);self.bearer_ref=None
 def wolf_count(self,player=None):
  player=self.owner if player is None else player
  return sum(t['alive'] and t['player']==player and 'Wolf' in t['subtypes'] for t in self.tokens)+sum(e['zone']=='battlefield' and e['player']==player and 'Wolf' in self.oracle[e['name']]['type_line'].split() for e in self.entities.values())
 def refresh_wolf_bonus(self):
  if not self.bearer_ref:return
  e=self.entities[self.bearer_ref];e.setdefault('base_power',e['power']);e.setdefault('base_toughness',e['toughness']);bonus=self.wolf_count() if self.active_source() and e['zone']=='battlefield' else 0;e['power']=e['base_power']+bonus;e['toughness']=e['base_toughness']+bonus
 def create(self,definition,amount,player=None,origin='source'):
  super().create(definition,amount,player,origin);self.refresh_wolf_bonus()
 def move(self,ref,zone):super().move(ref,zone);self.refresh_wolf_bonus()
 def drop_token(self,token,dies=True):super().drop_token(token,dies);self.refresh_wolf_bonus()
 def source_cost(self,p,mode,kicker):
  from token_trigger_creation_spec import wave6_cost
  cost=wave6_cost(self.oracle[p['source_name']])
  if p['wolf_kind']=='two_color_cast' and mode=='payment':cost=[3,0,3,0,0,0]
  if p['wolf_kind']=='multikicker_wolves':cost[3]+=kicker
  return cost
 def cast_wolf(self,p,mode,kicker=2):
  own=self.owner;kind=p['wolf_kind'];cost=self.source_cost(p,mode,kicker)
  if kind=='aura_wolf_lord':self.emit(_variable_step('require_can_cast',ref='source',value=1),_variable_step('choose_target',player=own,ref=self.bearer_ref))
  self.kicker_cut=len(self.steps)
  if kind=='multikicker_wolves':
   self.emit(*(_variable_step('choose_dialog',player=own,value=0) for _ in range(kicker)))
   if self.mana[own][3]>cost[3]:self.emit(_variable_step('choose_dialog',player=own,value=1))
  self.emit(_variable_step('cast',ref='source',value=1));self.pending_cut=len(self.steps);self.pay(own,cost);self.emit(_variable_step('assert_stack',value=1),_variable_step('assert_zone_multiplicity',player=own,zone='hand',index=self.rows[p['source_name']]['csvid'],value=1));self.emit(*(_variable_step('assert_mana',player=own,color=i,value=v) for i,v in enumerate(self.mana[own])));self.emit(_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));self.move('source','battlefield' if kind in ('multikicker_wolves','aura_wolf_lord','two_color_cast') else 'graveyard')
  if kind=='three_bodies':
   for component in p['tokens']:self.create(component['token'],component['count'])
  elif kind=='forest_wolves':self.create(p['token'],sum(e['zone']=='battlefield' and e['player']==own and 'Forest' in self.oracle[e['name']]['type_line'].split() for e in self.entities.values()))
  elif kind=='morbid_wolves':self.create(p['token'],3 if any(d['serial']==self.serial for d in self.deaths) else 1)
  elif kind=='multikicker_wolves':self.create(p['token'],kicker)
  elif kind=='aura_wolf_lord':self.create(p['token'],2)
 def fable_spell(self,name,ref,player=None,target=None):
  player=self.owner if player is None else player;active=self.active_source();self.emit(_variable_step('turn',player=player,value=0));self.turn=player;self.spell(name,ref,target,player=player)
  if 'Creature' in self.oracle[name]['type_line']:self.move(ref,'battlefield')
  if name=='Reach Through Mists':
   drawn=self.zones['library'][player].pop(0);self.zones['hand'][player].append(drawn)
   for e in self.entities.values():
    if e['zone']=='library' and e['player']==player and e['name']==drawn:e['zone']='hand';break
  symbols=set(re.findall(r'\{([BUGRW])\}',self.oracle[name]['mana_cost']))
  if player==self.owner and active:
   for component in self.profile['tokens']:
    if component['token']['name']=='Wolf' and 'G' in symbols or component['token']['name']=='Bird' and 'U' in symbols:self.create(component['token'],1)
 def bearer_proof(self):
  if self.bearer_ref and self.entities[self.bearer_ref]['zone']=='battlefield':
   e=self.entities[self.bearer_ref];self.emit(_variable_observe(self.bearer_ref,'power',e['power']),_variable_observe(self.bearer_ref,'toughness',e['toughness']),_variable_observe(self.bearer_ref,'plus1_counters',0))


def expand_wave6_wolves(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_wolf_variants,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_wolf_variants(p) for v in selected):raise ValueError('unique supported Wolf variants required')
 if any(v.startswith('prior_turn_expired_') for v in selected):
  out=[]
  for v in selected:out.extend(expand_wave6_morbid_actual_turns(name,csvid,d,p,[v],rows,oracle) if v.startswith('prior_turn_expired_') else expand_wave6_wolves(name,csvid,d,p,[v],rows,oracle))
  return out
 out=[]
 for requested in selected:
  variant,owner=requested.rsplit('_',1);owner=int(owner);mode={'ready_snapshot':'create','event_snapshot':'create','kicker_snapshot':'create','grant_snapshot':'bearer_attack'}.get(variant,variant);b=Wave6WolfLedger(owner,oracle,rows,p,data);b.add(name,'source');b.add('Ashcoat Bear','unselected',1-owner if mode=='opposing_bearer' else owner,'battlefield');kind=p['wolf_kind'];kicker=0 if mode=='zero_kicker' else 3 if mode=='larger_kicker' else 2
  for component in p['tokens']:b.universe.add(component['token']['name'])
  if mode=='bird_evasion':b.add('Grizzly Bears','ground_blocker',1-owner,'battlefield')
  if mode in ('block_ground','block_flying','elephant_block'):b.add('Suntail Hawk' if mode=='block_flying' else 'Grizzly Bears','combat_witness',1-owner,'battlefield')
  if kind=='aura_wolf_lord':
   b.bearer_ref=b.add('Grizzly Bears','bearer',1-owner if mode=='opposing_bearer' else owner,'battlefield');b.add('Grizzly Bears','opposing_unselected',1-owner,'battlefield')
   if mode in ('normal_wolf','opponent_wolf','foreign_wolf'):b.add('Watchwolf','normal_wolf',1-owner if mode=='opponent_wolf' else owner,'battlefield',opponent_owned=mode=='foreign_wolf')
  if kind=='forest_wolves':
   n=0 if mode=='zero_input' else 4 if mode=='larger_amount' else 2;side=1-owner if mode=='opponent_input' else owner
   for i in range(n):b.add('Forest','forest_'+str(i),side,'battlefield',tapped=True)
   b.add('Plains','nonforest',owner,'battlefield',tapped=True)
   if mode=='ownership':b.add('Forest','foreign_forest',owner,'battlefield',opponent_owned=True,tapped=True);b.add('Forest','owned_opposing_forest',1-owner,'battlefield',opponent_owned=True,tapped=True)
  for who,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(who)+'_'+str(i),who,'library')
  b.emit(_variable_step('turn',player=owner,value=0),_variable_step('present_ai_dialogs',value=1));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);ready_cut=len(b.steps)
  if kind=='morbid_wolves':
   b.emit(_variable_step('reset_death_history'))
   if mode in ('token_death','opponent_token_death'):
    side=1-owner if mode=='opponent_token_death' else owner;b.witness_tokens('Raise the Alarm','history_tokens',side);token=next(t for t in b.tokens if t['alive'] and t['player']==side);b.emit(_variable_step('bind_permanent_copy',player=side,value=rows[token['name']]['csvid'],index=0,ref='history_token'),_variable_step('destroy',ref='history_token'),_variable_step('refresh'));b.drop_token(token)
   elif mode=='noncreature_death':b.add('Bone Saw','noncreature_death',owner,'battlefield');b.destroy('noncreature_death')
   elif mode!='zero_input':
    side=1-owner if mode=='opponent_input' else owner;b.add('Grizzly Bears','history_creature',side,'battlefield',opponent_owned=mode=='ownership');b.destroy('history_creature')
   if mode=='prior_input_removed':b.purge()
   if mode=='prior_turn_expired':b.emit(_variable_step('advance_turn'));b.serial+=1;b.turn=1-owner
  if kind=='forest_wolves' and mode=='prior_input_removed':b.destroy('forest_0')
  denied=mode=='insufficient_mana';cost=b.source_cost(p,mode,kicker)
  if mode=='payment' or denied or kind=='two_color_cast':b.pool(owner,[0]*6 if denied else cost)
  event_cut=len(b.steps)
  if denied:b.emit(_variable_step('cast',ref='source',value=0),_variable_step('assert_stack',value=0),_variable_step('assert_choices_consumed'))
  else:
   b.cast_wolf(p,mode,kicker)
   if kind=='two_color_cast':
    b.pool(owner,[30,10,10,10,10,10]);event_cut=len(b.steps)
    if mode=='source_removed':b.fable_spell('Simic Sky Swallower','positive_spell');b.proof_tokens();b.destroy('source')
    witness='Elvish Mystic' if mode in ('green_spell','source_removed') else 'Reach Through Mists' if mode in ('blue_spell','bird_evasion') else 'Walking Corpse' if mode=='wrong_spell' else 'Simic Sky Swallower';b.fable_spell(witness,'color_spell',1-owner if mode=='opponent_spell' else owner)
    if mode=='repeat':b.fable_spell('Elvish Mystic','second_color_spell')
   b.proof_tokens();b.bearer_proof();grant_cut=len(b.steps)
   if mode=='amount_locked':b.destroy('forest_0')
   if mode=='foreign_wolf':b.destroy('normal_wolf');b.bearer_proof()
   if mode=='wolf_removed':
    token=next(t for t in b.tokens if t['alive'] and t['name']=='Wolf');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows['Wolf']['csvid'],index=0,ref='lost_wolf'),_variable_step('destroy',ref='lost_wolf'),_variable_step('refresh'));b.drop_token(token);b.bearer_proof()
   if mode in ('aura_removed','aura_lost_attack'):b.destroy('source');b.bearer_proof()
   if mode=='bearer_removed':b.destroy('bearer');b.move('source','graveyard')
   if mode=='bounce_recast':
    b.spell('Boomerang' if kind=='aura_wolf_lord' else 'Unsummon','bounce_source','source');b.move('source','hand');b.emit(_variable_step('bind_hand',player=owner,value=csvid,ref='source'));b.cast_wolf(p,'create',1);b.proof_tokens();b.bearer_proof()
   if mode in ('bearer_attack','aura_lost_attack'):
    b.age();e=b.entities['bearer'];b.emit(_variable_step('turn',player=e['player'],value=0),_variable_step('begin_attackers'),_variable_step('attack',ref='bearer',value=1),_variable_step('finish_attackers'),_variable_step('begin_blockers'),_variable_step('advance_combat'),_variable_step('combat_damage'),_variable_step('resolve'),_variable_step('assert_choices_consumed'),_variable_step('end_combat'));e['tapped']=True;b.life[1-e['player']]-=e['power']
   primary=p['token']
   if mode in ('snake_attack','elephant_attack','elephant_block'):primary=next(c['token'] for c in p['tokens'] if c['token']['name']==('Snake' if mode=='snake_attack' else 'Elephant'))
   if mode=='bird_evasion':primary=next(c['token'] for c in p['tokens'] if c['token']['name']=='Bird')
   combat_mode='ready_attack' if mode in ('snake_attack','elephant_attack') else 'block_ground' if mode=='elephant_block' else 'flying_evasion' if mode=='bird_evasion' else mode
   if mode in ('fresh_attack','ready_attack','block_ground','block_flying','snake_attack','elephant_attack','elephant_block','bird_evasion') and any(t['alive'] and t['player']==owner and t['name']==primary['name'] for t in b.tokens):wave6_spell_combat(b,dict(p,token=primary),combat_mode)
   if mode in ('destroy_token','bounce_token','exile_token'):
    token=next(t for t in b.tokens if t['alive'] and t['player']==owner and t['name']=='Wolf');b.emit(_variable_step('bind_permanent_copy',player=owner,value=rows['Wolf']['csvid'],index=0,ref='removed_token'))
    if mode=='destroy_token':b.emit(_variable_step('destroy',ref='removed_token'),_variable_step('refresh'))
    else:
     removal='Unsummon' if mode=='bounce_token' else 'Swords to Plowshares'
     if kind=='two_color_cast':b.fable_spell(removal,'token_removal',target='removed_token')
     else:b.spell(removal,'token_removal','removed_token')
     if mode=='exile_token':b.life[owner]+=token['power']
    b.drop_token(token,dies=mode=='destroy_token')
   if mode=='cleanup':b.emit(_variable_step('cleanup'),_variable_step('refresh'))
  b.finish();steps=b.steps
  if variant.endswith('snapshot'):
   cut=ready_cut if variant=='ready_snapshot' else b.pending_cut if variant=='kicker_snapshot' else grant_cut if variant=='grant_snapshot' else event_cut if kind=='two_color_cast' else b.pending_cut
   tail=steps[cut:];steps=steps[:cut]+[_variable_step('capture')]+copy.deepcopy(tail)+[_variable_step('record_state'),_variable_step('ai_roundtrip'),_variable_step('restore')]+copy.deepcopy(tail)+[_variable_step('assert_recorded_state')]
  gaps=['token replacement effects, multiplayer and controller changes after triggers','multiple blockers and damage assignment choices']
  if kind=='two_color_cast':gaps+=['both accepted creation components; separate green/blue optional-decline decisions untested','all-green hybrid payment and all-blue payment variant; mixed hybrid allocations untested']
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out


def expand_wave6_resolution(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,wave6_cost,wave6_resolution_variants,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if not selected or len(set(selected))!=len(selected) or any(v not in wave6_resolution_variants(p) for v in selected):raise ValueError('unique supported normal-resolution variants required')
 out=[]
 for requested in selected:
  mode,owner=requested.rsplit('_',1);owner=int(owner);kind=p['mechanism'];wolf=p.get('wolf_kind');breadth=kind in ('spell_count','spell_x','fixed_followup');b=(Wave6WolfLedger if kind=='wolf_creator' else Wave6SpellLedger if breadth else Wave6TokenLedger)(owner,oracle,rows,p,data);b.add(name,'source');b.add('Grizzly Bears','ordinary',owner,'battlefield');b.universe.add(p['token']['name'])
  for component in p.get('tokens',[]):b.universe.add(component['token']['name'])
  for player,names in ((0,['Forest','Island','Grizzly Bears']),(1,['Plains','Mountain','Ashcoat Bear'])):
   for i,nm in enumerate(names):b.add(nm,'library_'+str(player)+'_'+str(i),player,'library')
  if wolf=='aura_wolf_lord':b.bearer_ref='ordinary'
  b.emit(_variable_step('turn',player=owner,value=0),_variable_step('present_ai_dialogs',value=1));b.pool(0,b.mana[0]);b.pool(1,b.mana[1]);x=5 if kind=='x_wrath' else 3;kicker=2
  if breadth:wave6_spell_prepare(b,p,'create')
  if kind=='x_wrath':
   b.add('Ashcoat Bear','opposing_creature',1-owner,'battlefield');b.witness_tokens('Raise the Alarm','independent_before_wrath',1-owner);b.proof_tokens()
  if kind=='death_history' or wolf=='morbid_wolves':
   b.emit(_variable_step('reset_death_history'));b.add('Ashcoat Bear','history_creature',owner,'battlefield');b.destroy('history_creature')
  cost=b.source_cost(p,'create',kicker) if kind=='wolf_creator' else wave6_cost(oracle[name],x)
  if wolf=='aura_wolf_lord':b.emit(_variable_step('require_can_cast',ref='source',value=1),_variable_step('choose_target',player=owner,ref=b.bearer_ref))
  if '{X}' in oracle[name]['mana_cost']:b.emit(_variable_step('choose_x',player=owner,value=x if name=='Decree of Justice' else x*oracle[name]['mana_cost'].count('{X}')))
  if wolf=='multikicker_wolves':
   b.emit(*(_variable_step('choose_dialog',player=owner,value=0) for _ in range(kicker)))
   if b.mana[owner][3]>cost[3]:b.emit(_variable_step('choose_dialog',player=owner,value=1))
  if mode=='countered':
   b.add('Counterspell','counter',0);b.emit(_variable_step('choose_priority',ref='counter'),_variable_step('cast',ref='source',value=0),_variable_step('resolve'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(owner,cost);b.pay(0,variable_cost(oracle['Counterspell']));b.move('source','graveyard');b.move('counter','graveyard')
  else:
   b.emit(_variable_step('cast',ref='source',value=1));b.pay(owner,cost);b.emit(_variable_step('assert_stack',value=1),_variable_observe('source','in_hand',0),_variable_step('assert_zone_multiplicity',player=owner,zone='hand',index=csvid,value=1),*(_variable_step('assert_mana',player=owner,color=c,value=v) for c,v in enumerate(b.mana[owner])))
   b.add('Murder','target_removal',owner);b.emit(_variable_step('choose_target',player=owner,ref=b.bearer_ref),_variable_step('cast_response',ref='target_removal',value=1),_variable_step('assert_stack',value=2),_variable_step('resolve_response'),_variable_step('refresh'),_variable_step('assert_choices_consumed'));b.pay(owner,variable_cost(oracle['Murder']));b.move('target_removal','graveyard');b.move(b.bearer_ref,'graveyard');b.move('source','graveyard')
  b.emit(_variable_step('assert_stack',value=0));b.finish()
  gaps=['normal-cost source resolution only; atomic Flashback response windows and committed cast-trigger history are unclaimed','token replacement effects, multiplayer and multiple response spells','Oracle resolution effects are absent after actual countering or loss of the sole Aura target']+['inactive: '+line for line in p['inactive_clauses']]
  out.append(dict(name=('TokenTriggerCreationRecipe_' if p['creature'] else 'TokenSpellExtensionRecipe_')+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=b.steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out


def expand_wave6_morbid_actual_turns(name,csvid,d,p,selected,rows,oracle):
 from token_trigger_creation_spec import wave6_data,validate_wave6_witnesses
 data=wave6_data();validate_wave6_witnesses(oracle,rows,p)
 if p.get('wolf_kind')!='morbid_wolves' or not selected or len(set(selected))!=len(selected) or any(v not in ('prior_turn_expired_0','prior_turn_expired_1') for v in selected):raise ValueError('reviewed actual-turn Morbid components required')
 out=[]
 for requested in selected:
  owner=int(requested.rsplit('_',1)[1]);b=Wave6WolfLedger(owner,oracle,rows,p,data);b.add(name,'source');b.add('Grizzly Bears','history_creature',owner,'battlefield');b.universe.add('Wolf')
  for player in (0,1):
   for i in range(12):b.add('Forest','library_'+str(player)+'_'+str(i),player,'library')
  b.emit(_variable_step('advance_turn'),_variable_step('turn',player=owner,value=0));b.turn=owner;b.emit(_variable_step('reset_death_history'));b.destroy('history_creature');b.emit(_variable_step('assert_card',ref='history_creature',property='in_play',value=0),_variable_step('assert_zone_multiplicity',player=owner,zone='graveyard',index=rows['Grizzly Bears']['csvid'],value=1))
  for player in (0,1):b.pool(player,[0]*6)
  b.emit(_variable_step('assert_stack',value=0),*(_variable_step('assert_zone',player=player,zone='battlefield',value=0) for player in (0,1)),_variable_step('assert_zone_multiplicity',player=owner,zone='hand',index=csvid,value=1),_variable_step('ai_turns',value=2),_variable_step('refresh'),_variable_step('assert_choices_consumed'))
  for player in (0,1):
   b.move('library_'+str(player)+'_0','hand' if player==0 else 'battlefield');b.emit(_variable_step('assert_zone',player=player,zone='battlefield',value=int(player==1)),_variable_step('assert_zone',player=player,zone='library',value=11),_variable_step('assert_zone_multiplicity',player=player,zone='battlefield',index=rows['Forest']['csvid'],value=int(player==1)),_variable_step('assert_zone_multiplicity',player=player,zone='hand',index=rows['Forest']['csvid'],value=int(player==0)),_variable_step('assert_zone_multiplicity',player=player,zone='hand',index=csvid,value=int(player==owner)))
  b.serial+=2;b.turn=owner;b.emit(_variable_step('turn',player=owner,value=0),_variable_step('bind_hand',player=owner,value=csvid,ref='source'));b.pool(owner,[3,0,0,1,0,0]);b.cast_wolf(p,'create');cut=len(b.steps);b.finish();b.steps[cut:]=[a for a in b.steps[cut:] if not(a['op']=='assert_card' and a.get('ref')=='history_creature')]
  gaps=['the quiet deterministic AI path draws one basic each: AI0 retains its basic and AI1 plays its basic; neither can afford3G before exact funding','broader AI choices and multiple full-turn triggers are untested; original dead creature refs are proven before AI and then only by graveyard identity because AI can recycle slots','bounded advance_turn alone does not reset the native Morbid summary; this component uses actual run_duel_turn/init_turn','multiple blockers, token replacement effects and multiplayer']
  out.append(dict(name='TokenSpellExtensionRecipe_'+str(csvid)+'_wave6_'+requested,card=name,fixture='empty_main',seed=123,setup=b.setup,steps=b.steps,_recipe=dict(family=p['family'],version=2,variant=requested,params=d['params'],oracle_text=d['oracle_text'],untested_components=gaps)))
 return out
