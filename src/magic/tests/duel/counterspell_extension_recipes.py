"""Reviewed counterspell components beyond ordinary type/color counters.

Source profiles pin complete Oracle text and independently authored semantics.
The ordinary counter recipe supplies real casting, target and conservation
receipts; this module adds printed secondary effects, costs and relationships.
"""
import copy
import json
from pathlib import Path
import counter_recipes as ordinary

PINS = json.loads((Path(__file__).parent/'data/counterspell_extension_witnesses.json').read_text(encoding='utf8'))
PROFILES = PINS['profiles']
TOKEN_ALIASES = PINS.get('token_aliases', {})


def spec(d, oracle):
    from recipes import cost_from_oracle
    name = oracle.get('name') if isinstance(oracle, dict) else None
    profile = PROFILES.get(name)
    if (not profile or oracle.get('card_faces') or oracle.get('selected_face')
            or d.get('family') != 'counterspell_extension' or d.get('version') != 2
            or d.get('params') != profile['params'] or d.get('oracle_text') != profile['source']['oracle_text']
            or any(oracle.get(k) != v for k,v in profile['source'].items())
            or not isinstance(d.get('variants'), list) or not d['variants']):
        raise ValueError('counter extension requires explicit reviewed parameters, full source pin and variants')
    result = copy.deepcopy(profile['params'])
    if result.get('extension',{}).get('wave')==5 and json.dumps(d['params'],sort_keys=True)!=json.dumps(result,sort_keys=True):
        raise ValueError('reviewed wave-five parameter types required')
    if result.get('extension',{}).get('resource_counter'):return _spec_resource_counter(d,result,oracle)
    if result.get('extension',{}).get('delayed_counter'):return _spec_delayed_counter(d,result,oracle)
    mana_oracle=oracle
    if result.get('extension',{}).get('variable_x'):
        if oracle.get('mana_cost','').count('{X}')!=1:raise ValueError('reviewed single X cost required')
        mana_oracle=dict(oracle,mana_cost=oracle['mana_cost'].replace('{X}',''))
    if result.get('extension',{}).get('colorless_cost'):
        if oracle.get('mana_cost','').count('{C}')!=1:raise ValueError('reviewed dedicated colorless cost required')
        mana_oracle=dict(oracle,mana_cost=oracle['mana_cost'].replace('{C}','{1}'))
    result.update(family='counterspell_extension', mana=cost_from_oracle(mana_oracle))
    if result.get('extension',{}).get('ability_counter'):
        return _spec_activated_counter(d,result)
    if 'extension' in result:
        available = _variants_new(result)
        if (any(not isinstance(v,str) for v in d['variants']) or len(set(d['variants'])) != len(d['variants'])
                or any(v not in available for v in d['variants'])):
            raise ValueError('unknown or duplicate new counter extension variant')
        ordinary.spec(_new_counter_params(result),result['mana'])
        return result
    q = _ordinary_params(result)
    ordinary.spec(q, result['mana'])
    available = variants(result)
    if (any(not isinstance(v,str) for v in d['variants'])
            or len(set(d['variants'])) != len(d['variants']) or any(v not in available for v in d['variants'])):
        raise ValueError('unknown or duplicate counter extension variant')
    return result


def _ordinary_params(p, branch=None):
    q = copy.deepcopy(p['counter'])
    if q.pop('draw_from_value', False): q['draw'] = 2
    if 'target_damage' in q: q['target_lose_life'] = q.pop('target_damage')
    relation = p.get('relationship')
    if relation:
        # Restrict fixtures, not the printed target definition. A permanent
        # target uses Giant Growth; a player target uses Ancestral Recall.
        q['targets'] = ['instant']
        q['colors'] = ['U'] if relation == 'player' or branch == 'player_target' else ['G']
        if branch == 'aura_target':
            q['targets']=['aura'];q['colors']=[]
    return q


def variants(p):
    if p.get('extension',{}).get('delayed_counter'):return _variants_delayed_counter(p)
    if p.get('extension',{}).get('resource_counter'):return _variants_resource_counter(p)
    if p.get('extension',{}).get('ability_counter'):return _variants_activated_counter(p)
    if 'extension' in p:return _variants_new(p)
    q = _ordinary_params(p)
    base = ordinary.variants(q)
    if any(gap.startswith('Surge alternative cost:') for gap in p['inactive_components']):
        # Countering the controller's own previously cast spell would enable
        # surge and would leave the declared normal-cost-only fixture scope.
        base.remove('own_spell')
    if p['relationship']:
        base = ['opponent','own_spell','payment','insufficient_mana','no_stack','snapshot','wrong_relationship']
        if p['relationship']=='you_or_your_permanent': base += ['player_target']
        if p['relationship']!='player':base += ['aura_target']
    if p['life_cost']: base += ['insufficient_life']
    if p['counter'].get('draw_from_value') or p['token'] and p['token']['from_value']:
        base += ['value_high']
    # Both actual source controllers exercise every reviewed branch.
    return [v+'_'+str(owner) for owner in (0,1) for v in base]


def validate_witnesses(oracle_context, registry):
    if not isinstance(oracle_context,dict):raise ValueError('pinned witness Oracle context required')
    ordinary.validate_witnesses(oracle_context,registry)
    if PINS.get('ability_witnesses'):_validate_activated_counter_witnesses(oracle_context,registry)
    for name, pin in PINS['cards'].items():
        if name not in registry or any(oracle_context.get(name,{}).get(k)!=v for k,v in pin.items()):
            raise ValueError('counter extension witness changed: '+name)
    for profile in PROFILES.values():
        token=profile['params']['token']
        if token:
            alias=TOKEN_ALIASES.get(token['name'])
            if alias:
                if registry.get(alias['registry_name'],{}).get('csvid')!=alias['csvid']:
                    raise ValueError('counter extension legacy token registry alias changed')
            elif token['name'] not in registry or not oracle_context.get(token['name'],{}).get('type_line','').startswith('Token '):
                raise ValueError('registered counter extension token identity required')


def _swap_players(case):
    for c in case['setup']: c['player']=1-c['player']
    seen=set()
    for s in case['steps']:
        # The ordinary snapshot recipe deliberately reuses its effect/check
        # objects on replay. Swap each object once, not once per occurrence.
        if id(s) in seen:continue
        seen.add(id(s))
        if s.get('player') in (0,1):s['player']=1-s['player']
        if s['op']=='choose_player':s['value']=1-s['value']


def expand(name, csvid, d, p, selected, registry, oracle_context):
    if p.get('extension',{}).get('delayed_counter'):return _expand_delayed_counter(name,csvid,d,p,selected,registry,oracle_context)
    if p.get('extension',{}).get('resource_counter'):return _expand_resource_counter(name,csvid,d,p,selected,registry,oracle_context)
    if p.get('extension',{}).get('ability_counter'):return _expand_activated_counter(name,csvid,d,p,selected,registry,oracle_context)
    if 'extension' in p:return _expand_new(name,csvid,d,p,selected,registry,oracle_context)
    from recipes import step, observe
    result=[]
    for requested in selected:
        branch,owner=requested.rsplit('_',1);owner=int(owner)
        mapped={'wrong_relationship':'insufficient_mana','insufficient_life':'insufficient_mana','player_target':'opponent','aura_target':'opponent','value_high':'creature_power'}.get(branch,branch)
        q=_ordinary_params(p,branch)
        # Ask the common fixture builder which independently reviewed spell it
        # selected, then derive only quantities explicitly printed as mana value.
        case=ordinary.expand(name,csvid,d,dict(q,mana=p['mana']),[mapped],registry,oracle_context)[0]
        victim=next(c['name'] for c in case['setup'] if c['ref']=='spell')
        value=int(oracle_context[victim]['cmc'])
        if p['counter'].get('draw_from_value'):
            q['draw']=value
            case=ordinary.expand(name,csvid,d,dict(q,mana=p['mana']),[mapped],registry,oracle_context)[0]
        negative=branch in ('no_stack','insufficient_mana','wrong_type','wrong_color','wrong_value','wrong_relationship','insufficient_life')
        source_resolved=not negative
        target_owner=int(mapped!='own_spell')
        relation=p['relationship']
        if relation and relation!='player':
            recipient=next(c for c in case['setup'] if c['ref']=='recipient')
            recipient['player']=1 if branch=='wrong_relationship' else 0
            if branch=='player_target':
                for s in case['steps']:
                    if s['op']=='choose_player':s['value']=0
        if relation=='player' and branch=='wrong_relationship':
            # An instant targeting a creature is an independently legal spell,
            # but is not a legal Outwit target. Keep all resolution receipts.
            q['colors']=['G']
            case=ordinary.expand(name,csvid,d,dict(q,mana=p['mana']),['insufficient_mana'],registry,oracle_context)[0]
            victim='Giant Growth'
        if branch in ('wrong_relationship','insufficient_life'):
            case['steps']=[s for s in case['steps'] if s['op']!='set_mana']
            for s in case['steps']:
                if s['op']=='assert_mana_total' and s['player']==0:s['value']=60
                if branch=='wrong_relationship' and s['op']=='cast' and s.get('ref')=='source':s['op']='can_cast'
        if p['life_cost']:
            if branch=='insufficient_life':case['steps'].insert(0,step('set_life',player=0,value=2))
            for s in case['steps']:
                if s['op']=='assert_life' and s['player']==0:
                    s['value']=2 if branch=='insufficient_life' else 20-(p['life_cost'] if source_resolved else 0)
        token=p['token']
        if token:
            # A legacy registry spelling is only an identity bridge. Printed
            # characteristics continue to come from the pinned maker text.
            registry_name=TOKEN_ALIASES.get(token['name'],{}).get('registry_name',token['name'])
            token_id=registry[registry_name]['csvid']
            token_owner=target_owner if token['recipient']=='spell_controller' else 0
            count=(value if token['count'] is None else token['count']) if source_resolved else 0
            def token_checks():
                checks=[step('assert_zone_multiplicity',player=who,zone='battlefield',index=token_id,value=count if who==token_owner else 0) for who in (0,1)]
                if count:
                    props=dict(token=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in token['colors']),artifact=int(token['artifact']))
                    if token['power'] is not None or token['name']=='Ooze':
                        props.update(power=value if token['from_value'] else token['power'],toughness=value if token['from_value'] else token['toughness'])
                    props.update({'subtype_'+s:1 for s in token['subtypes']})
                    checks += [step('assert_matching_permanents',player=token_owner,index=token_id,property=k,value=v) for k,v in props.items()]
                    for i in range(count):
                        ref='created_'+str(i)
                        checks += [step('bind_permanent_copy',player=token_owner,value=token_id,index=i,ref=ref),observe(ref,'tapped',0)]
                return checks
            # Include the token receipts on both sides of the common snapshot
            # replay boundary, so rollback/recreation cannot duplicate tokens.
            out=[]
            for s in case['steps']:
                if s['op'] in ('record_state','assert_recorded_state'):out += token_checks()
                out.append(s)
            case['steps']=out+token_checks()
        if owner:
            _swap_players(case)
            if source_resolved:
                # The legacy opponent response menu filters by carddata's
                # act_phases, which is zero for these instant records. Cast
                # both real spells onto the pending stack explicitly instead.
                # Costs, target selection and resolution stay in the engine.
                actions=[]
                for action in case['steps']:
                    if action['op']=='choose_priority' and action.get('ref')=='source':continue
                    if action['op']=='cast' and action.get('ref')=='spell':
                        action['value']=1
                        actions += [action,step('assert_stack',value=1),step('cast_response',ref='source',value=1),step('assert_stack',value=2)]
                    else:
                        if action['op']=='resolve':action['op']='resolve_response'
                        actions.append(action)
                case['steps']=actions
            elif branch!='no_stack':
                # Pending-stack negative casts need the same real response
                # context. A bare CAN_CAST query after the first cast would
                # otherwise reject solely because its response context ended.
                for action in case['steps']:
                    if action.get('ref')=='source' and action['op'] in ('cast','can_cast'):
                        action['op']='cast_response'
        gaps=list(p['inactive_components'])+['Counter removal during response, multiple simultaneous stack objects, replacement effects and multiplayer.']
        if token:
            gaps += ['Token control changes, copy and doubling effects.']
            if token['flying']:gaps += ['Created token flying: token combat is not exercised.']
        case['name']=f'CounterExtensionRecipe_{csvid}_{requested}'
        case['_recipe']=dict(family='counterspell_extension',version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=gaps)
        result.append(case)
    return result

# New reviewed profiles opt into this builder. The legacy profile path above
# intentionally remains byte-for-byte stable for its already validated homes.
def _new_counter_params(p):
    q=copy.deepcopy(p['counter'])
    if p['extension'].get('fixture_targets'):q['targets']=p['extension']['fixture_targets']
    if p['relationship']=='you':q.update(targets=['instant'],colors=['U'])
    if p['extension']['tax'] or p['extension'].get('variable_x') is True:q['tax']=2
    return q


def _variants_new(p):
    e=p['extension'];q=_new_counter_params(p);base=ordinary.variants(q)
    if e['mode_dialogs'] is not None:
        base=[v for v in base if v not in ('no_stack','wrong_type','wrong_color','wrong_value')]
    if e['tax']:
        base += ['value_high'] if e['tax']['basis']=='mana_value' else ['count_none','count_many']
    if e['additional_cost']:base += ['no_cost_resource']
    if e['untap']:base += ['untap_none','untap_opponent']
    if e.get('optional_target_draw'):base += ['draw_declined']
    if e.get('counter_pump'):
        base += ['bonus_declined','bonus_cleanup']
        if e['counter_pump']['controller']=='any':base.append('bonus_opponent')
        else:base.append('bonus_no_owned')
    if p['token']:base.append('token_mana')
    if e.get('condition') or e.get('resolution_condition')=='maximum_permanent_value' or e.get('hard_counter_condition'):base += ['condition_none','condition_opponent']
    if e.get('condition')=='spell_mastery':base.append('mastery_snapshot')
    if e.get('resolution_condition') in ('maximum_permanent_value','mana_value_limit'):base.append('value_high')
    if e.get('counter_color'):base.append('color_match')
    if e.get('kicker'):base += ['kicked','kicked_high','kicked_above','kicked_payment']
    if e.get('secondary'):
        base += ['secondary_self','secondary_land'] if e['secondary']['action']=='bounce' else ['secondary_self','secondary_lethal','secondary_cleanup']
    if e.get('variable_x') is True:base += ['x_0','x_1','x_2','x_5','x_8']
    elif e.get('variable_x')=='mana_value':base.append('value_high')
    if e.get('buyback'):base += ['buyback','buyback_payment']
    if e.get('destination_choice'):base.append('bottom')
    if e.get('followup_cast_denial'):base += ['denial_expiry','other_player_cast']
    if e.get('black_payment_mill'):base += ['black_payment','black_high','black_snapshot','black_own_spell']
    if e.get('damage_alternative'):base += ['damage_taken','damage_own_spell','damage_payment','damage_sorcery']
    if e.get('acquire_types'):base += ['acquired_mana','acquired_owner_grave']
    if e.get('free_matching'):base += ['free_declined','free_nonmatching']
    if e.get('grave_shuffle'):base += ['grave_declined','grave_timing']
    if e.get('poison_condition'):base += ['condition_none','condition_opponent']
    if e.get('search_same_name'):
        base += ['search_none','search_many']
        if e['search_same_name']['delirium']:base += ['delirium_three','delirium_opponent','delirium_own_parent','search_declined','search_partial']
    if e.get('mastery_copy'):base += ['mastery_none','mastery_one','mastery_opponent','mastery_own_parent','copy_declined','copy_opponent']
    if e.get('proliferate'):base += ['proliferate_none','proliferate_cleanup']
    if e.get('clash'):base += ['clash_lose','clash_tie','clash_bottom_source','clash_bottom_target']
    if any(p['mana'][1:]) or e.get('colorless_cost'):base.append('wrong_mana')
    result=[v+'_'+str(owner) for owner in (0,1) for v in base]
    if e.get('proliferate'):result += ['proliferate_partial_0','proliferate_declined_0']
    return result


def _new_tax(e,setup,stock,value,o):
    t=e['tax']
    if not t:return None
    basis=t['basis'];own=[c for c in setup if c['player']==0]
    battlefield=[c for c in own if c['zone']=='battlefield']
    if basis=='mana_value':return value
    if basis=='artifacts':return sum('Artifact' in o[c['name']]['type_line'] for c in battlefield)
    if basis=='hand':return stock+sum(c['zone']=='hand' and c['ref'] not in ('source','spell') for c in own)
    if basis=='graveyard':return stock+sum(c['zone']=='graveyard' for c in own)
    if basis=='devotion_blue':return sum(o[c['name']]['mana_cost'].count('{U}') for c in battlefield)
    if basis=='power':return max([int(o[c['name']]['power']) for c in battlefield if 'Creature' in o[c['name']]['type_line']]+[0])
    if basis=='domain':return len({typ for c in battlefield for typ in ('Forest','Island','Plains','Mountain','Swamp') if typ in o[c['name']]['type_line']})
    if basis=='rune_snag':return 2+2*sum(c['zone']=='graveyard' and c['name']=='Rune Snag' for c in setup)
    raise ValueError('unreviewed derived counter tax')


def _new_auxiliary(e,branch):
    from recipes import card
    out=copy.deepcopy(e['auxiliary']);t=e['tax'];none=branch=='count_none';many=branch=='count_many'
    if t and t['basis']!='mana_value':
        count=0 if none else t.get('count',2)+(1 if many else 0)
        basis=t['basis']
        for i in range(count):
            nm,zone=('Ornithopter','battlefield') if basis=='artifacts' else ('Forest','hand') if basis=='hand' else ('Forest','graveyard') if basis=='graveyard' else ('Merfolk of the Pearl Trident','battlefield') if basis=='devotion_blue' else ('Hill Giant','battlefield') if basis=='power' else (('Forest','Plains','Island','Mountain','Swamp')[i],'battlefield') if basis=='domain' else ('Rune Snag','graveyard')
            item=card(nm,'tax_'+str(i),0,zone)
            if zone=='battlefield' and basis=='domain':item['state']=['tapped']
            out.append(item)
        if basis=='rune_snag' and not none:out.append(card('Rune Snag','tax_opponent',1,'graveyard'))
    if e['additional_cost'] and branch!='no_cost_resource':
        cost=e['additional_cost'];item=card(cost['witness'],'cost_resource')
        if cost['filter'] in ('land','permanent'):item['state']=['tapped']
        out.append(item)
    for i in range(e['untap']):
        item=card(('Forest','Island','Plains','Mountain')[i],'untap_'+str(i),1 if branch=='untap_opponent' else 0);item['state']=['tapped'];out.append(item)
    if e['untap']:
        item=card(('Forest','Island','Plains','Mountain','Swamp')[e['untap']],'untap_unselected',1 if branch=='untap_opponent' else 0);item['state']=['tapped'];out.append(item)
    if e.get('counter_pump'):
        out += [card('Wind Drake','bonus_recipient'),card('Hill Giant','bonus_other',1)]
    if e.get('condition')=='blue_creature':
        if branch!='condition_none':out.append(card('Wind Drake','condition_blue',1 if branch=='condition_opponent' else 0))
    if e.get('condition')=='spell_mastery' and branch!='snapshot':
        for i,n in enumerate(('Giant Growth','Lightning Bolt')[:1 if branch=='condition_none' else 2]):
            out.append(card(n,'condition_grave_'+str(i),1 if branch=='condition_opponent' else 0,'graveyard'))
    if e.get('secondary'):
        nm='Forest' if branch=='secondary_land' else 'Wind Drake' if branch=='secondary_lethal' else 'Giant Spider'
        item=card(nm,'secondary',0 if branch=='secondary_self' else 1)
        if nm=='Forest':item['state']=['tapped']
        out += [item,card('Hill Giant','secondary_control',1-item['player'])]
    if e.get('resolution_condition')=='maximum_permanent_value':
        out.append(card('Ornithopter' if branch in ('condition_none','condition_opponent') else 'Grizzly Bears','condition_permanent'))
        if branch=='condition_opponent':out.append(card('Hill Giant','condition_opponent',1))
    if e.get('hard_counter_condition'):
        hard=branch not in ('condition_none','condition_opponent','tax_paid','tax_exact','tax_unaffordable')
        if e['hard_counter_condition']=='ferocious':out.append(card('Scaled Wurm' if hard else 'Hill Giant','condition_power'))
        elif hard:out.append(card('Shivan Dragon','condition_dragon'))
        if branch=='condition_opponent':out.append(card('Scaled Wurm' if e['hard_counter_condition']=='ferocious' else 'Shivan Dragon','condition_opponent',1))
    if e.get('mastery_copy'):
        count=0 if branch=='mastery_none' else 1 if branch in ('mastery_one','mastery_own_parent') else 2
        out += [card(n,'copy_mastery_'+str(i),1 if branch=='mastery_opponent' else 0,'graveyard') for i,n in enumerate(('Giant Growth','Lightning Bolt')[:count])]
    if e.get('proliferate'):
        out += [card('Grizzly Bears','proliferate_positive'),card('Hill Giant','proliferate_negative',1)]
        if branch!='proliferate_none':out += [card('Hunger of the Howlpack','proliferate_seed_plus',0,'hand'),card('Virulent Wound','proliferate_seed_minus',0,'hand')]
    return out


def _expand_new(name,csvid,d,p,selected,rows,o):
    from collections import Counter
    from recipes import step,observe,cost_from_oracle
    from grant_recipes import HAND,GRAVE,EXILE
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected))!=len(selected) or any(v not in _variants_new(p) for v in selected)):
        raise ValueError('new counter extension expansion requires unique reviewed variants')
    result=[];e=p['extension']
    for requested in selected:
        branch,owner=requested.rsplit('_',1);owner=int(owner)
        mapped={'value_high':'creature_power','count_none':'opponent','count_many':'opponent','no_cost_resource':'opponent','untap_none':'opponent','untap_opponent':'opponent','wrong_mana':'insufficient_mana','draw_declined':'opponent','bonus_declined':'opponent','bonus_cleanup':'opponent','bonus_opponent':'opponent','bonus_no_owned':'opponent','token_mana':'opponent','condition_none':'opponent','condition_opponent':'opponent','mastery_snapshot':'snapshot','secondary_self':'opponent','secondary_land':'opponent','secondary_lethal':'opponent','secondary_cleanup':'opponent'}.get(branch,'opponent' if branch.startswith('x_') else branch)
        q=_new_counter_params(p)
        if e.get('mastery_copy') and branch in ('mastery_none','mastery_one','mastery_opponent','copy_declined','copy_opponent'):mapped='opponent'
        if e.get('mastery_copy') and branch=='mastery_own_parent':mapped='own_spell'
        if e.get('proliferate') and branch.startswith('proliferate_'):mapped='opponent'
        if e.get('clash') and branch.startswith('clash_'):mapped='opponent'
        if branch in ('acquired_mana','acquired_owner_grave'):mapped='type_artifact'
        if branch in ('free_declined','grave_declined','grave_timing'):mapped='opponent'
        if branch=='free_nonmatching':mapped='type_instant'
        if branch in ('buyback','buyback_payment','bottom','denial_expiry','other_player_cast','black_payment'):mapped='opponent'
        if branch=='black_snapshot':mapped='snapshot'
        if branch=='black_own_spell':mapped='own_spell'
        if branch=='black_high':mapped='creature_power'
        if branch in ('damage_taken','damage_payment'):mapped='opponent'
        if branch=='damage_own_spell':mapped='own_spell'
        if branch=='damage_sorcery':mapped='type_sorcery'
        if branch in ('search_none','search_many','delirium_three','delirium_opponent','search_declined','search_partial'):mapped='opponent'
        if branch=='delirium_own_parent':mapped='own_spell'
        if branch=='color_match':mapped='opponent';q['colors']=[e['counter_color']]
        if e.get('destination_choice') and branch=='bottom':q['destination']='library_bottom'
        if branch in ('kicked','kicked_payment'):mapped='opponent'
        if branch in ('kicked_high','kicked_above'):mapped='creature_power'
        template=ordinary.expand(name,csvid,d,dict(q,mana=p['mana']),[mapped],rows,o)[0]
        setup=copy.deepcopy(template['setup']);full=template['fixture']=='stocked_zones';stock=4 if full else 0
        if branch=='kicked_above':next(c for c in setup if c['ref']=='spell')['name']='Scaled Wurm'
        setup+=_new_auxiliary(e,branch)
        if e.get('clash'):
            tops=('Forest','Hill Giant') if branch=='clash_lose' else ('Forest','Island') if branch=='clash_tie' else ('Scaled Wurm','Grizzly Bears' if branch=='clash_bottom_target' else 'Forest')
            for player,top in enumerate(tops):
                library=[c for c in setup if c['zone']=='library' and c['player']==player]
                if library:library[0]['name']=top
        if e.get('loot') and e['loot']['player']=='target':
            for c in setup:
                if c['ref']=='discard_fodder':c['player']=next(c['player'] for c in setup if c['ref']=='spell')
        if (e['additional_cost'] and e['additional_cost']['filter'] in ('creature','permanent')
                or branch=='count_none' and e['tax'] and e['tax']['basis']=='power'):
            for c in setup:
                if c['ref']=='recipient':c['player']=1
        if branch=='bonus_no_owned':
            for c in setup:
                if c['ref'] in ('recipient','bonus_recipient'):c['player']=1
        if e.get('resolution_condition')=='maximum_permanent_value' or e.get('hard_counter_condition'):
            for c in setup:
                if c['ref']=='recipient':c['player']=1
        source_failed=branch in ('insufficient_mana','wrong_mana','wrong_type','wrong_color','wrong_value','no_cost_resource')
        no_stack=branch=='no_stack';source_resolved=not source_failed and not no_stack
        spell=next(c for c in setup if c['ref']=='spell');recipient=next(c for c in setup if c['ref']=='recipient')
        if e.get('followup_cast_denial'):
            from recipes import card
            setup += [card('Grizzly Bears','followup',spell['player'],'hand'),card('Giant Growth','unaffected_spell',1-spell['player'],'hand')]
        if branch=='acquired_owner_grave':
            from recipes import card
            setup.append(card('Disenchant','owner_test_removal',0,'hand'))
        victim=spell['name'];target_owner=spell['player'];category=next((w[1] for w in ordinary.WITNESSES if w[0]==victim),'creature' if 'Creature' in o[victim]['type_line'] else None);value=int(o[victim]['cmc'])
        poison=e.get('poison_condition')
        poisoned_player=(1-target_owner if branch=='condition_opponent' else target_owner) if poison and branch!='condition_none' and not no_stack else None
        if poison:
            from recipes import card
            setup += [card('Virulent Wound','poison_spell',0,'hand'),card('Suntail Hawk','poison_victim',target_owner if poisoned_player is None else poisoned_player)]
        search=e.get('search_same_name')
        if search:
            from recipes import card
            other=1-target_owner
            setup += [card(victim,'search_other_hand',other,'hand'),card(victim,'search_other_grave',other,'graveyard'),card(victim,'search_other_library',other,'library'),card('Island','search_hand_control',target_owner,'hand'),card('Forest','search_grave_control',target_owner,'graveyard')]
            if category=='creature':setup.append(card(victim,'search_battlefield_control',target_owner))
            if branch!='search_none':
                copies=2 if branch=='search_many' else 1
                setup += [card(victim,'search_hand_'+str(i),target_owner,'hand') for i in range(copies)]
                setup += [card(victim,'search_grave_'+str(i),target_owner,'graveyard') for i in range(copies)]
                setup += [card(victim,'search_library_'+str(i),target_owner,'library') for i in range(copies+1)]
            else:
                for c in setup:
                    if c['player']==target_owner and c['zone']=='library' and c['name']==victim:c['name']='Island'
            if search['delirium']:
                names=['Grizzly Bears','Forest'] if branch=='delirium_three' else ['Grizzly Bears','Forest','Sol Ring'] if branch=='delirium_own_parent' else ['Grizzly Bears','Forest','Sol Ring','Lightning Bolt']
                setup += [card(n,'delirium_'+str(i),1 if branch=='delirium_opponent' else 0,'graveyard') for i,n in enumerate(names)]
        x=value if e.get('variable_x')=='mana_value' else int(branch[2:]) if branch.startswith('x_') else 3
        source_cost=list(p['mana'])
        if e.get('variable_x'):source_cost[0]+=x
        bought=bool(e.get('buyback') and branch.startswith('buyback'))
        if bought:source_cost[0]+=e['buyback']
        black_spent=bool(e.get('black_payment_mill') and branch.startswith('black_'))
        if black_spent:source_cost[0]-=1;source_cost[1]+=1
        kicked=bool(e.get('kicker') and branch.startswith('kicked'))
        if kicked:source_cost[0]+=e['kicker']
        tax=_new_tax(e,setup,stock,value,o)
        if tax is None:tax=q.get('tax')
        if e.get('variable_x') is True:tax=x
        hard=(any(c['player']==0 and c['zone']=='battlefield' and 'Creature' in o[c['name']]['type_line'] and int(o[c['name']]['power'])>=4 for c in setup) if e.get('hard_counter_condition')=='ferocious' else
              any(c['player']==0 and c['zone']=='battlefield' and 'Dragon' in o[c['name']]['type_line'] for c in setup) if e.get('hard_counter_condition')=='dragon' else False)
        if hard:tax=None
        condition=(any(c['zone']=='battlefield' and c['player']==0 and 'Creature' in o[c['name']]['type_line'] and 'U' in o[c['name']]['colors'] for c in setup) if e.get('condition')=='blue_creature' else
                   (sum(any(t in o[n]['type_line'] for t in ('Instant','Sorcery')) for n in GRAVE) if full else 0)+sum(c['zone']=='graveyard' and c['player']==0 and any(t in o[c['name']]['type_line'] for t in ('Instant','Sorcery')) for c in setup)>=2 if e.get('condition')=='spell_mastery' else True)
        paid=bool(tax is not None and (tax==0 or branch in ('tax_paid','tax_exact')))
        if e.get('damage_alternative'):paid=branch.startswith('damage_')
        resolution_permitted=(poisoned_player==target_owner if poison else e['counter_color'] in o[victim]['colors'] if e.get('counter_color') else
                              value<=max([int(o[c['name']]['cmc']) for c in setup if c['player']==0 and c['zone']=='battlefield']+[0]) if e.get('resolution_condition')=='maximum_permanent_value' else
                              value<=(4 if kicked else 2) if e.get('resolution_condition')=='mana_value_limit' else True)
        countered=source_resolved and not paid and resolution_permitted
        acquired=bool(countered and category in e.get('acquire_types',[]))
        free_selected=bool(source_resolved and e.get('free_matching') and category=='creature' and branch!='free_declined')
        grave_selected=bool(source_resolved and e.get('grave_shuffle') and branch!='grave_declined')
        original_resolved=not no_stack and not countered
        pools=[[10]*6,[10]*6];target_cost=cost_from_oracle(o[victim]);life=[20,20]
        pre=[step('turn',player=target_owner,value=0)]
        if e.get('black_payment_mill'):
            pools[0][0]=20;pools[0][1]=0
            pre += [step('set_mana',player=0,color=0,value=20),step('set_mana',player=0,color=1,value=0)]
        if e.get('loot'):pre.append(step('discard_policy',value=1))
        proliferate_seeded=bool(e.get('proliferate') and branch!='proliferate_none')
        if branch in ('payment','kicked_payment','buyback_payment','damage_payment','insufficient_mana','wrong_mana') or black_spent:
            pools[0]=list(source_cost) if branch!='insufficient_mana' else [0]*6
            if black_spent and target_owner==0:pools[0]=[a+b for a,b in zip(pools[0],target_cost)]
            if branch=='wrong_mana':
                color=0 if e.get('colorless_cost') else next(i for i in range(1,6) if pools[0][i]);pools[0][1 if color!=1 else 3]+=pools[0][color];pools[0][color]=0
            if poison and poisoned_player is not None:pools[0][1]+=1
            if proliferate_seeded:pools[0][1]+=1;pools[0][3]+=1
            pre += [step('set_mana',player=0,color=c,value=n) for c,n in enumerate(pools[0])]
        if branch in ('tax_exact','tax_unaffordable'):
            pools[target_owner]=list(target_cost);pools[target_owner][0]+=tax if branch=='tax_exact' else 0
            pre += [step('set_mana',player=target_owner,color=c,value=n) for c,n in enumerate(pools[target_owner])]
        def pay(player,cost):pools[player]=[a-b for a,b in zip(pools[player],cost)]
        zones=[dict(hand=list(HAND) if full else [],graveyard=list(GRAVE) if full else [],exile=list(EXILE) if full else [],library=list(ordinary.LIBRARY) if full else [],battlefield=[]) for _ in (0,1)]
        for c in setup:zones[c['player']][c['zone']].append(c['name'])
        copy_mastery=(sum(any(t in o[n]['type_line'] for t in ('Instant','Sorcery')) for n in zones[0]['graveyard'])+int(countered and target_owner==0 and category in ('instant','sorcery')))>=2
        copy_active=bool(e.get('mastery_copy') and source_resolved and copy_mastery)
        copy_selected=copy_active and branch!='copy_declined';copy_player=int(branch=='copy_opponent')
        clash_won=bool(e.get('clash') and source_resolved and float(o[zones[0]['library'][0]]['cmc'])>float(o[zones[1]['library'][0]]['cmc']))
        search_active=bool(search and source_resolved)
        if search_active and search['delirium']:
            grave=list(zones[0]['graveyard'])+([victim] if countered and target_owner==0 else [])
            kinds={t for n in grave for t in ('Artifact','Creature','Enchantment','Instant','Land','Planeswalker','Sorcery','Tribal') if t in o[n]['type_line']}
            search_active=len(kinds)>=4
        search_counts={z:zones[target_owner][z].count(victim)+(int(countered) if z=='graveyard' else -int(not no_stack) if z=='hand' else 0) for z in ('hand','graveyard','library')}
        selected_search={z:(0 if branch=='search_declined' else int(z=='graveyard') if branch=='search_partial' else n) for z,n in search_counts.items()}
        states={c['ref']:dict(alive=c['zone']=='battlefield',hand=c['zone']=='hand',tapped='tapped' in c.get('state',[])) for c in setup if c['zone'] in ('hand','battlefield')}
        def move(ref,destination):
            c=next(c for c in setup if c['ref']==ref);state=states[ref];old='battlefield' if state['alive'] else 'hand' if state['hand'] else None
            if old:zones[c['player']][old].remove(c['name'])
            zones[c['player']][destination].append(c['name']);state['alive']=destination=='battlefield';state['hand']=destination=='hand'
        def draw(player,count):
            cards=zones[player]['library'][:count];del zones[player]['library'][:count];zones[player]['hand']+=cards
        pump=e.get('counter_pump');bonus_ref='bonus_other' if branch=='bonus_opponent' else 'bonus_recipient'
        actions=[]
        if poison or e.get('proliferate'):pre.insert(0,step('reset_death_history'))
        if proliferate_seeded:
            for spell_ref,target_ref,n in (('proliferate_seed_plus','proliferate_positive','Hunger of the Howlpack'),('proliferate_seed_minus','proliferate_negative','Virulent Wound')):
                actions += [step('choose_target',player=0,ref=target_ref),step('cast',ref=spell_ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0)]
                pay(0,cost_from_oracle(o[n]));move(spell_ref,'graveyard')
                # Zone identities survive these resolved instants; old slot
                # references need not survive a subsequent legacy allocation.
                del states[spell_ref]
            actions += [observe('proliferate_positive','plus1_counters',1),observe('proliferate_positive','power',3),observe('proliferate_negative','minus1_counters',1),observe('proliferate_negative','toughness',2)]
        if poisoned_player is not None:
            actions += [step('choose_target',player=0,ref='poison_victim'),step('cast',ref='poison_spell',value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0),observe('poison_victim','in_play',0),step('assert_poison',player=poisoned_player,value=1),step('assert_poison',player=1-poisoned_player,value=0)]
            pay(0,cost_from_oracle(o['Virulent Wound']));move('poison_spell','graveyard');move('poison_victim','graveyard')
        if no_stack:actions += [step('can_cast',ref='source',value=0)]
        else:
            if victim in ('Giant Growth','Lightning Bolt','Pacifism'):actions.append(step('choose_target',player=target_owner,ref='recipient'))
            if victim=='Ancestral Recall':actions.append(step('choose_player',player=target_owner,value=0 if p['relationship']=='you' else target_owner))
            actions += [step('cast',ref='spell',value=1),step('assert_stack',value=1)];pay(target_owner,target_cost)
            if source_resolved:
                if e.get('present_ai_dialogs'):actions.append(step('present_ai_dialogs',value=1))
                actions += [step('choose_dialog',player=0,value=n) for n in (e['mode_dialogs'] or {}).get(str(owner),[])]
                if e.get('normal_buyback') and stock+sum(c['zone']=='hand' and c['player']==0 and c['ref'] not in ('source','spell') for c in setup)>=2:
                    actions.append(step('choose_dialog',player=0,value=0))
                # The sacrifice helper selects its sole legal resource itself
                # for controller one. Its actual sacrifice and mana receipts
                # are still asserted; only a nonexistent UI input is omitted.
                if e['additional_cost'] and (e['additional_cost']['action']!='sacrifice' or owner==0):
                    actions.append(step('choose_target',player=0,ref='cost_resource'))
                if pump:actions += [step('choose_cancel_target',player=0)] if branch=='bonus_declined' else [step('choose_target',player=0,ref=bonus_ref)]
                if e.get('secondary'):actions.append(step('choose_target',player=0,ref='secondary'))
                if e.get('variable_x') is True:actions.append(step('choose_x',player=0,value=x))
                if e.get('buyback') and sum(pools[0])-sum(p['mana'])-x>=e['buyback']:actions.append(step('choose_dialog',player=0,value=0 if bought else 1))
                if black_spent and owner==0:actions.append(step('choose_dialog',player=0,value=1))
                if e.get('kicker') and sum(pools[0])-sum(p['mana'])>=e['kicker']:actions.append(step('choose_dialog',player=0,value=0 if kicked else 1))
                if e.get('grave_shuffle'):actions.append(step('choose_zone',player=0,value=-1) if branch=='grave_declined' else step('choose_zone_card',player=0,value=rows['Forest']['csvid']))
                actions += [step('cast_response',ref='source',value=1),step('assert_stack',value=2)];pay(0,source_cost)
                if branch=='grave_timing':actions += [step('assert_choices_consumed'),step('choose_zone_card',player=0,value=rows['Forest']['csvid'])]
                if e.get('free_matching') and category=='creature':actions.append(step('choose_zone',player=0,value=-1) if branch=='free_declined' else step('choose_zone_card',player=0,value=rows['Hill Giant']['csvid']))
                if tax and branch!='tax_unaffordable':actions.append(step('choose_dialog',player=target_owner,value=0 if paid else 1))
                if e.get('mastery_copy'):
                    actions.append(step('choose_dialog',player=0,value=1 if copy_active and not copy_selected else 0))
                    if copy_selected or not copy_active:actions.append(step('choose_player',player=0,value=copy_player))
                if e.get('proliferate') and owner==0:
                    actions.append(step('choose_dialog',player=0,value=1))
                    if branch!='proliferate_none':
                        chosen=[] if branch=='proliferate_declined' else ['proliferate_positive'] if branch=='proliferate_partial' else ['proliferate_positive','proliferate_negative']
                        for ref in chosen:actions += [step('choose_target',player=0,ref=ref),step('choose_dialog',player=0,value=0)]
                        if len(chosen)<2:actions.append(step('choose_cancel_target',player=0))
                if e.get('clash'):
                    actions += [step('choose_dialog',player=0,value=0),step('choose_dialog',player=0,value=0)]
                    for player in (0,1):
                        bottom=branch==('clash_bottom_source' if player==0 else 'clash_bottom_target')
                        actions.append(step('choose_zone_card',player=player,value=rows[zones[player]['library'][0]]['csvid']) if bottom else step('choose_zone',player=player,value=-1))
                if e.get('damage_alternative'):actions.append(step('choose_dialog',player=target_owner,value=0 if paid else 1))
                if e.get('optional_target_draw'):actions.append(step('choose_dialog',player=target_owner,value=1 if branch=='draw_declined' else 0))
                if q.get('scry') and condition:actions += [step('choose_zone',player=0,value=-1)]*(1 if owner else q['scry'])
                if e.get('destination_choice'):actions.append(step('choose_dialog',player=0,value=1 if branch=='bottom' else 0))
                if e.get('rearrange_top') and owner==0:actions += [step('choose_zone',player=0,value=2),step('choose_zone',player=0,value=-1)]
                if search_active:
                    for z in ('hand','graveyard','library'):
                        actions += [step('choose_zone_card',player=0,value=rows[victim]['csvid']) for _ in range(selected_search[z])]
                        if selected_search[z]<search_counts[z] or z!='graveyard' and search_counts[z]==0:actions.append(step('choose_zone',player=0,value=-1))
                if e['untap']:
                    actions += [step('choose_cancel_target',player=0)] if branch=='untap_none' else [step('choose_target',player=0,ref='untap_'+str(i)) for i in range(e['untap'])]
                # Above four, this AI handler rejects a spell that Oracle
                # permits casting. Isolate that actual-cast permission probe
                # with an ordinary parent drain; functional high-value
                # resolution is checked with the faithful response helper for
                # controller zero, not inferred from this fallback path.
                permission_only=branch=='kicked_above' and owner==1 or bool(poison and poisoned_player!=target_owner)
                actions += [step('resolve' if permission_only else 'resolve_response'),step('assert_unused_choices',value=0) if permission_only else step('assert_unused_choices',value=2) if e.get('mastery_copy') and not copy_active else step('assert_unused_choices',value=1) if branch in ('bonus_no_owned','grave_timing') else step('assert_choices_consumed')]
                if e.get('present_ai_dialogs'):actions.append(step('present_ai_dialogs',value=0))
            else:
                # A rejected response must not consume its optional bonus
                # target. Preplanning a legal bonus also lets a wrong parent
                # permission finish without turning that rule defect into an
                # unscripted-input harness failure.
                if pump:actions.append(step('choose_target',player=0,ref='bonus_recipient'))
                if e.get('damage_alternative'):actions.append(step('choose_dialog',player=target_owner,value=1))
                actions += [step('cast_response',ref='source',value=0),step('assert_stack',value=1),step('resolve')]
                actions.append(step('assert_unused_choices',value=1) if pump or e.get('damage_alternative') else step('assert_choices_consumed'))
        if source_resolved:
            move('source','hand' if bought else 'graveyard')
            if free_selected:move('free_spell','battlefield')
            if e['additional_cost']:move('cost_resource','graveyard' if e['additional_cost']['action']=='sacrifice' else 'hand')
            if e.get('secondary'):
                second=next(c for c in setup if c['ref']=='secondary')
                if e['secondary']['action']=='bounce':move('secondary','hand')
                elif int(o[second['name']]['toughness'])<=e['secondary']['amount']:move('secondary','graveyard')
        if not no_stack:
            if acquired:
                zones[target_owner]['hand'].remove(victim);zones[0]['battlefield'].append(victim);states['spell'].update(hand=False,alive=True,tapped=False)
            elif countered:
                destination=q.get('destination','graveyard')
                if destination.startswith('library_'):
                    zones[target_owner]['hand'].remove(victim);states['spell']['hand']=False
                    zones[target_owner]['library'].insert(0,victim) if destination=='library_top' else zones[target_owner]['library'].append(victim)
                else:move('spell',destination)
            else:move('spell','graveyard' if category in ('instant','sorcery') else 'battlefield')
        if grave_selected:zones[0]['graveyard'].remove('Forest');zones[0]['library'].append('Forest')
        discard=None
        if search_active:
            for z in ('hand','graveyard','library'):
                for _ in range(selected_search[z]):zones[target_owner][z].remove(victim);zones[target_owner]['exile'].append(victim)
            if selected_search['hand']:
                for c in setup:
                    if c['player']==target_owner and c['zone']=='hand' and c['name']==victim and c['ref'] in states:states[c['ref']]['hand']=False
        if source_resolved:
            draw(0,q.get('draw',0));draw(target_owner,0 if e.get('optional_target_draw') and branch=='draw_declined' else q.get('target_draw',0));life[0]+=q.get('gain_life',0);life[target_owner]-=q.get('target_lose_life',0)
            if e.get('x_gain_life'):life[0]+=x
            if e.get('x_damage'):life[target_owner]-=x
            if e.get('damage_alternative') and paid:life[target_owner]-=e['damage_alternative']
            if e.get('loot') and condition:
                loot=e['loot'];player=0 if loot['player']=='self' else target_owner;draw(player,loot['draw'])
                if zones[player]['hand']:
                    discard=dict(player=player,count=min(loot['discard'],len(zones[player]['hand'])),hand=Counter(zones[player]['hand']),grave=Counter(zones[player]['graveyard']))
            if q.get('mill'):
                cards=zones[target_owner]['library'][:q['mill']];del zones[target_owner]['library'][:q['mill']];zones[target_owner]['graveyard']+=cards
            if black_spent:
                cards=zones[target_owner]['library'][:value];del zones[target_owner]['library'][:value];zones[target_owner]['graveyard']+=cards
            if e.get('rearrange_top') and owner==0:
                library=zones[0]['library'];library.insert(0,library.pop(2))
            if paid and tax:pools[target_owner][0]-=tax
        if copy_selected:draw(copy_player,3)
        if e.get('drain_mana_lands') and countered:
            pools[target_owner]=[0]*6
            for c in setup:
                if c['player']==target_owner and c['zone']=='battlefield' and c['name'] in e['drain_mana_lands']:states[c['ref']]['tapped']=True
        if e.get('clash') and source_resolved:
            for player,label in ((0,'clash_bottom_source'),(1,'clash_bottom_target')):
                if branch==label:zones[player]['library'].append(zones[player]['library'].pop(0))
            if clash_won:
                milled=zones[target_owner]['library'][:4];del zones[target_owner]['library'][:4];zones[target_owner]['graveyard']+=milled
        if original_resolved and victim in ('Divination','Ancestral Recall'):draw(0 if victim=='Ancestral Recall' and p['relationship']=='you' else target_owner,2 if victim=='Divination' else 3)
        token=p['token'];token_id=None
        if token:
            token_id=rows[TOKEN_ALIASES.get(token['name'],{}).get('registry_name',token['name'])]['csvid']
            if source_resolved:zones[0]['battlefield'] += [token['name']]*token['count']
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        if poison:checks += [step('assert_poison',player=player,value=int(player==poisoned_player)) for player in (0,1)]
        if acquired:checks.append(step('bind_permanent_copy',player=0,value=rows[victim]['csvid'],index=0,ref='spell'))
        for c in setup:
            if c['ref'] not in states:continue
            ref=c['ref'];state=states[ref]
            if (ref in ('cost_resource','secondary') or ref=='source' and bought) and state['hand'] and source_resolved:checks.append(step('bind_hand',player=c['player'],value=rows[c['name']]['csvid'],ref=ref))
            # Discard is a choice among legal hand cards. Zone cardinalities,
            # per-identity bounds and hand/graveyard conservation below cover
            # the result without asserting that a particular hand ref survived.
            discard_candidate=discard and discard['player']==c['player'] and state['hand'] and discard['hand'][c['name']]
            checks += ([observe(ref,'in_play',int(state['alive']))]+([] if discard_candidate else [observe(ref,'in_hand',int(state['hand']))])) if ref not in ('source','spell') else [observe(ref,'in_hand',int(state['hand']))] if ref=='source' else [observe(ref,'in_play',int(state['alive']))]
            if state['alive']:
                tapped=state['tapped'] and not (e['untap'] and ref in {'untap_'+str(i) for i in range(e['untap'])} and source_resolved and branch!='untap_none')
                checks.append(observe(ref,'tapped',int(tapped)))
                if 'Creature' in o[c['name']]['type_line']:
                    power=int(o[c['name']]['power']);toughness=int(o[c['name']]['toughness']);damage=0
                    if ref=='recipient' and original_resolved:
                        if victim=='Giant Growth':power+=3;toughness+=3
                        if victim=='Lightning Bolt':damage=3
                        if victim=='Glorious Anthem' and c['player']==target_owner:power+=1;toughness+=1
                    if ref=='secondary' and source_resolved and e['secondary']['action']=='damage':damage=e['secondary']['amount']
                    counters=pump['amount'] if pump and source_resolved and branch not in ('bonus_declined','bonus_no_owned') and ref==bonus_ref else 0
                    minus=0
                    if e.get('proliferate') and ref in ('proliferate_positive','proliferate_negative'):
                        selected=source_resolved and branch not in ('proliferate_none','proliferate_declined') and (ref=='proliferate_positive' or branch!='proliferate_partial')
                        amount=int(proliferate_seeded)+int(selected)
                        if ref=='proliferate_positive':counters=amount
                        else:minus=amount;power-=minus;toughness-=minus
                    checks += [observe(ref,'power',power+counters),observe(ref,'toughness',toughness+counters),observe(ref,'damage',damage),observe(ref,'plus1_counters',counters),observe(ref,'minus1_counters',minus)]
        if token and source_resolved:
            props=dict(token=1,color=sum(1<<{'B':1,'U':2,'G':3,'R':4,'W':5}[c] for c in token['colors']),artifact=int(token['artifact']),power=token['power'],toughness=token['toughness'])
            props.update({'subtype_'+s:1 for s in token['subtypes']})
            checks += [step('assert_matching_permanents',player=0,index=token_id,property=k,value=v) for k,v in props.items()]
            checks += [step('bind_permanent_copy',player=0,value=token_id,index=0,ref='created_token'),observe('created_token','tapped',0),observe('created_token','damage',0)]
        universe=sorted({c['name'] for c in setup}|set(HAND+GRAVE+EXILE+ordinary.LIBRARY))
        if token:universe=sorted(set(universe)|{token['name']})
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(pools[player]))]
            if branch in ('payment','kicked_payment','buyback_payment','damage_payment') or black_spent:checks += [step('assert_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
            for zone,names in zones[player].items():
                change=(-discard['count'] if zone=='hand' else discard['count'] if zone=='graveyard' else 0) if discard and discard['player']==player else 0
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)+change))
                if zone=='library':
                    # The AI performs its permitted top-card ordering without
                    # a scripted reorder prompt. Pin the unaffected suffix and
                    # all library identities, allowing the selected top window
                    # to be any Oracle-permitted permutation.
                    window=q.get('scry',0) if condition else 0
                    if e.get('rearrange_top'):window=e['rearrange_top']
                    reorder=bool(owner and player==0 and source_resolved and window>1)
                    shuffled=search_active and player==target_owner or grave_selected and player==0
                    checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(names) if not shuffled and (not reorder or i>=window)]
                    if reorder or shuffled:checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=Counter(names)[n]) for n in universe]
                else:
                    counts=Counter(names)
                    for n in universe:
                        if discard and discard['player']==player and zone in ('hand','graveyard') and discard['hand'][n]:
                            maximum=discard['hand'][n] if zone=='hand' else discard['grave'][n]+min(discard['count'],discard['hand'][n])
                            checks.append(step('assert_zone_multiplicity_max',player=player,zone=zone,index=rows[n]['csvid'],value=maximum))
                        else:checks.append(step('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=counts[n]))
            if discard and discard['player']==player:
                checks += [step('assert_hand_graveyard_total',player=player,index=rows[n]['csvid'],value=discard['hand'][n]+discard['grave'][n]) for n in universe]
        if e.get('followup_cast_denial') and source_resolved:
            checks += [step('cast',ref='followup',value=0),step('assert_stack',value=0),observe('followup','in_hand',1),step('assert_choices_consumed')]
        steps=pre+actions+checks
        if branch=='bonus_cleanup':
            chosen=next(c for c in setup if c['ref']==bonus_ref);amount=pump['amount']
            steps += [step('cleanup'),step('refresh'),observe(bonus_ref,'plus1_counters',amount),observe(bonus_ref,'power',int(o[chosen['name']]['power'])+amount),observe(bonus_ref,'toughness',int(o[chosen['name']]['toughness'])+amount)]
        if branch=='secondary_cleanup':steps += [step('cleanup'),step('refresh'),observe('secondary','damage',0),observe('secondary','in_play',1)]
        if e.get('followup_cast_denial') and source_resolved:
            if branch=='denial_expiry':
                steps += [step('cleanup'),step('turn',player=target_owner,value=0)]+[step('set_mana',player=target_owner,color=i,value=n) for i,n in enumerate(cost_from_oracle(o['Grizzly Bears']))]
                steps += [step('cast',ref='followup',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),observe('followup','in_play',1),observe('followup','in_hand',0),step('assert_mana_total',player=target_owner,value=0)]
                steps += [step('assert_zone',player=target_owner,zone=z,value=len(zones[target_owner][z])+delta) for z,delta in [('hand',-1),('battlefield',1)]]
                steps += [step('assert_zone_multiplicity',player=target_owner,zone=z,index=rows['Grizzly Bears']['csvid'],value=zones[target_owner][z].count('Grizzly Bears')+delta) for z,delta in [('hand',-1),('battlefield',1)]]
            if branch=='other_player_cast':
                other=1-target_owner
                steps += [step('choose_target',player=other,ref='recipient'),step('cast',ref='unaffected_spell',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed'),observe('recipient','power',int(o[recipient['name']]['power'])+3),observe('recipient','toughness',int(o[recipient['name']]['toughness'])+3),observe('unaffected_spell','in_hand',0),step('assert_mana_total',player=other,value=sum(pools[other])-1)]
                steps += [step('assert_zone_multiplicity',player=other,zone=z,index=rows['Giant Growth']['csvid'],value=zones[other][z].count('Giant Growth')+delta) for z,delta in [('hand',-1),('graveyard',1)]]
        if branch=='token_mana':
            steps += [step('activate',ref='created_token',value=1),step('assert_stack',value=0),observe('created_token','in_play',0),step('assert_mana_total',player=0,value=sum(pools[0])+1)]
            steps += [step('assert_zone_multiplicity',player=player,zone=zone,index=token_id,value=0) for player in (0,1) for zone in ('hand','battlefield','graveyard','exile')]
        if branch=='acquired_mana':
            steps += [step('assert_matching_permanents',player=0,index=rows['Sol Ring']['csvid'],property='artifact',value=1),step('can_activate',ref='spell',value=1),step('activate',ref='spell',value=1),step('assert_stack',value=0),observe('spell','tapped',1),step('assert_mana_total',player=0,value=sum(pools[0])+2)]
            steps += [step('assert_mana',player=0,color=color,value=n+int(color==0)*2) for color,n in enumerate(pools[0])]
        if branch=='acquired_owner_grave':
            steps += [step('set_mana',player=0,color=color,value=int(color in (0,5))) for color in range(6)]
            steps += [step('choose_target',player=0,ref='spell'),step('cast',ref='owner_test_removal',value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed'),observe('spell','in_play',0),step('assert_mana_total',player=0,value=0),step('assert_zone_multiplicity',player=0,zone='graveyard',index=rows['Disenchant']['csvid'],value=1)]
            steps += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows['Sol Ring']['csvid'],value=int(player==target_owner and zone=='graveyard')) for player in (0,1) for zone in ('hand','battlefield','graveyard','exile')]
        if branch=='proliferate_cleanup':
            steps += [step('cleanup'),observe('proliferate_positive','plus1_counters',2),observe('proliferate_positive','power',4),observe('proliferate_positive','toughness',4),observe('proliferate_negative','minus1_counters',2),observe('proliferate_negative','power',1),observe('proliferate_negative','toughness',1)]
        if branch in ('snapshot','mastery_snapshot','black_snapshot'):steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
        case=dict(name=f'CounterExtensionRecipe_{csvid}_{requested}',card=name,fixture=template['fixture'],seed=123,setup=setup,steps=steps,
                  _recipe=dict(family='counterspell_extension',version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=list(p['inactive_components'])+['Countered response source, lost targets, replacement/prevention, multiple simultaneous responses and multiplayer.']))
        if branch=='kicked_above' and owner==1:case['_recipe']['untested_components'].append('This controller-one above-four case independently probes actual casting permission; functional above-four resolution is deferred here because a rejected response leaves only the parent. Controller-zero exclusion uses the faithful response helper.')
        if poison and poisoned_player!=target_owner:case['_recipe']['untested_components'].append('Unpoisoned-controller cases independently assert actual casting permission and source payment/zone receipts. Functional unpoisoned resolution uses an ordinary drain because an erroneous rejected response leaves only the parent; poisoned positives use the faithful response helper.')
        if owner:_swap_players(case)
        result.append(case)
    return result


# Activated-counter component: only real activate/cast operations create pending objects.
def _activated_counter_config(p):
    e=p.get('extension',{});a=e.get('ability_counter')
    if isinstance(a,dict) and a.get('land_response'):
        if (set(e)!={'wave','ability_counter'} or e.get('wave')!=5
                or set(a)!={'allowed_sources','draw','disable_parent','default_parent','land_response','destroy_parent'}
                or a['allowed_sources']!=['any'] or type(a['draw']) is not int or a['draw']!=2
                or a['disable_parent'] is not False or a['default_parent']!='icy'
                or a['land_response'] is not True or a['destroy_parent'] is not True
                or p['counter'] is not None or p['token'] is not None or p['life_cost'] or p['relationship'] is not None):
            raise ValueError('explicit reviewed land-targeted counter component required')
        return a
    if (set(e)!={'wave','ability_counter'} or e.get('wave')!=5
            or not isinstance(a,dict) or set(a)!={'allowed_sources','draw','disable_parent','default_parent'}
            or a['allowed_sources'] not in (['any'],['artifact'],['artifact','creature','enchantment','land'])
            or type(a['draw']) is not int or a['draw'] not in (0,1)
            or type(a['disable_parent']) is not bool or a['default_parent'] not in ('whelp','icy')
            or p['counter'] is not None or p['token'] is not None or p['life_cost'] or p['relationship'] is not None):
        raise ValueError('explicit reviewed activated-counter component required')
    return a


def _spec_activated_counter(d,p):
    _activated_counter_config(p);available=_variants_activated_counter(p)
    declared=p.copy();declared.pop('family');declared.pop('mana')
    if json.dumps(d['params'],sort_keys=True)!=json.dumps(declared,sort_keys=True):
        raise ValueError('activated-counter parameter types must match the pin')
    if (not isinstance(d['variants'],list) or not d['variants']
            or any(not isinstance(v,str) for v in d['variants'])
            or len(set(d['variants']))!=len(d['variants'])
            or any(v not in available for v in d['variants'])):
        raise ValueError('unique reviewed activated-counter variants required')
    return p


def _variants_activated_counter(p):
    a=_activated_counter_config(p)
    if a.get('land_response'):
        return [v+'_'+str(owner) for owner in (0,1) for v in ('opponent','own_ability','payment','insufficient_mana','wrong_mana','no_stack','mana_ability','spell_parent','snapshot','control_activation','wrong_land','nonland_target','spell_land')]
    base=['opponent','own_ability','payment','insufficient_mana','wrong_mana',
          'no_stack','mana_ability','spell_parent','snapshot','control_activation']
    if a['default_parent']=='whelp':base += ['artifact_parent','repeat_parent','expiry']
    else:base.append('wrong_source')
    return [v+'_'+str(owner) for owner in (0,1) for v in base]


def _validate_activated_counter_witnesses(oracle,registry):
    for name,pin in PINS['ability_witnesses'].items():
        if name not in registry or any(oracle.get(name,{}).get(k)!=v for k,v in pin.items()):
            raise ValueError('activated-counter witness changed: '+name)


def _expand_activated_counter(name,csvid,d,p,selected,rows,oracle):
    from collections import Counter
    from recipes import card,step,observe,cost_from_oracle
    from grant_recipes import HAND,GRAVE,EXILE
    a=_activated_counter_config(p);available=_variants_activated_counter(p)
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected))!=len(selected) or any(v not in available for v in selected)):
        raise ValueError('unique reviewed activated-counter expansion selection required')
    _validate_activated_counter_witnesses(oracle,rows)
    cases=[]
    for variant in selected:
        branch,owner=variant.rsplit('_',1);owner=int(owner)
        parent_owner=0 if branch=='own_ability' else 1;other_owner=1-parent_owner
        full=branch=='snapshot';fixture='stocked_zones' if full else 'empty_main'
        actor='icy' if branch=='artifact_parent' else 'whelp' if branch=='wrong_source' else a['default_parent']
        land_response=a.get('land_response');land_ref='opposing_land' if branch=='wrong_land' else 'whelp' if branch=='nonland_target' else 'own_land'
        kind='none' if branch=='no_stack' else 'mana' if branch=='mana_ability' else 'spell' if branch in ('spell_parent','spell_land') else 'ability'
        allowed=(a['allowed_sources']==['any'] or ('artifact' if actor=='icy' else 'creature') in a['allowed_sources'])
        cast_allowed=kind=='ability' and allowed and branch not in ('insufficient_mana','wrong_mana')
        if land_response:cast_allowed=(kind=='ability' and parent_owner!=0 and land_ref=='own_land' or branch=='spell_land') and branch not in ('insufficient_mana','wrong_mana')
        setup=[card(name,'source',0,'hand'),card('Dragon Whelp','whelp',parent_owner),
               card('Icy Manipulator','icy',parent_owner),card('Dragon Whelp','other_whelp',other_owner),
               card('Sol Ring','mana_source',parent_owner),card('Grizzly Bears','parent_spell',parent_owner,'hand')]
        if land_response:
            setup += [card('Forest','own_land',0),card('Forest','opposing_land',1)]
            if branch=='spell_land':next(c for c in setup if c['ref']=='parent_spell')['name']='Stone Rain'
        if kind!='mana':next(c for c in setup if c['ref']=='mana_source')['state']=['tapped']
        if not full:setup += [card(n,f'library_{player}_{i}',player,'library') for player in (0,1) for i,n in enumerate(ordinary.LIBRARY)]
        pools=[[10]*6,[10]*6];pre=[step('turn',player=parent_owner,value=0)]
        if branch in ('payment','insufficient_mana','wrong_mana'):
            pools[0]=list(p['mana']) if branch!='insufficient_mana' else [0]*6
            if branch=='wrong_mana':
                color=next(i for i in range(1,6) if pools[0][i]);pools[0][1 if color!=1 else 3]+=pools[0][color];pools[0][color]=0
            pre += [step('set_mana',player=0,color=i,value=n) for i,n in enumerate(pools[0])]
        zones=[dict(hand=list(HAND) if full else [],graveyard=list(GRAVE) if full else [],exile=list(EXILE) if full else [],library=list(ordinary.LIBRARY) if full else [],battlefield=[]) for _ in (0,1)]
        states={}
        for c in setup:
            zones[c['player']][c['zone']].append(c['name'])
            if c['zone'] in ('hand','battlefield'):
                states[c['ref']]=dict(zone=c['zone'],tapped='tapped' in c.get('state',[]),power=int(oracle[c['name']].get('power',0) or 0))
        def pay(player,cost):pools[player]=[n-v for n,v in zip(pools[player],cost)]
        def move(ref,zone):
            c=next(c for c in setup if c['ref']==ref);s=states[ref]
            zones[c['player']][s['zone']].remove(c['name']);zones[c['player']][zone].append(c['name']);s['zone']=zone
        def mana_receipts(player):
            return [step('assert_mana_total',player=player,value=sum(pools[player]))]+[step('assert_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
        actions=[]
        if kind=='ability':
            if actor=='icy':actions.append(step('choose_target',player=parent_owner,ref=land_ref if land_response else 'whelp'))
            actions += [step('can_activate',ref=actor,value=1),step('activate',ref=actor,value=1),step('assert_stack',value=1)]
            pay(parent_owner,[1,0,0,0,0,0] if actor=='icy' else [0,0,0,0,1,0])
            if actor=='icy':states['icy']['tapped']=True
            actions += mana_receipts(parent_owner)+[observe('whelp','power',2),observe('whelp','toughness',3),observe('whelp','tapped',0),observe('icy','tapped',int(states['icy']['tapped']))]
        elif kind=='mana':
            actions += [step('can_activate',ref='mana_source',value=1),step('activate',ref='mana_source',value=1),step('assert_stack',value=0)]
            pools[parent_owner][0]+=2;states['mana_source']['tapped']=True
            actions += mana_receipts(parent_owner)+[observe('mana_source','tapped',1)]
        elif kind=='spell':
            if branch=='spell_land':actions.append(step('choose_target',player=parent_owner,ref='own_land'))
            actions += [step('cast',ref='parent_spell',value=1),step('assert_stack',value=1)]
            pay(parent_owner,cost_from_oracle(oracle['Stone Rain' if branch=='spell_land' else 'Grizzly Bears']))
        # Initial path to probe: an ordinary real instant cast above the actual
        # activation. No special response callback or manufactured effect is used.
        actions += [step('cast_response' if branch=='spell_land' else 'cast',ref='source',value=int(cast_allowed)),step('assert_stack',value=(2 if cast_allowed else 1) if kind in ('ability','spell') else 0)]
        if cast_allowed:
            pay(0,p['mana'])
            actions += mana_receipts(0)+[step('assert_zone',player=0,zone='hand',value=len(zones[0]['hand'])),step('assert_zone_multiplicity',player=0,zone='hand',index=csvid,value=1),step('assert_zone_multiplicity',player=0,zone='graveyard',index=csvid,value=0),step('assert_zone',player=0,zone='library',value=len(zones[0]['library']))]
        if kind in ('ability','spell'):actions.append(step('resolve_response' if branch=='spell_land' else 'resolve'))
        actions += [step('assert_stack',value=0),step('assert_choices_consumed')]
        if cast_allowed:
            move('source','graveyard')
            if land_response:
                move(actor,'graveyard') if kind=='ability' else move('parent_spell','graveyard')
            for _ in range(a['draw']):zones[0]['hand'].append(zones[0]['library'].pop(0))
        elif kind=='ability':
            if actor=='whelp':states['whelp']['power']+=1
            else:states[land_ref if land_response else 'whelp']['tapped']=True
        elif kind=='spell':move('parent_spell','battlefield')
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for c in setup:
            if c['ref'] not in states:continue
            s=states[c['ref']];ref=c['ref'];alive=s['zone']=='battlefield'
            checks += [observe(ref,'in_play',int(alive)),observe(ref,'in_hand',int(s['zone']=='hand'))]
            if alive:
                checks += [observe(ref,'tapped',int(s['tapped']))]
                if 'Creature' in oracle[c['name']]['type_line']:
                    checks += [observe(ref,'power',s['power']),observe(ref,'toughness',int(oracle[c['name']]['toughness'])),observe(ref,'damage',0),observe(ref,'plus1_counters',0),observe(ref,'minus1_counters',0)]
        if kind=='ability':
            parent_available=actor=='whelp' and not (cast_allowed and a['disable_parent'])
            checks += [step('can_activate',ref=actor,value=int(parent_available))]
            if actor=='icy':checks.append(step('can_activate',ref='whelp',value=1))
        checks.append(step('can_activate',ref='other_whelp',value=int(pools[other_owner][4]>=1)))
        universe=sorted({c['name'] for c in setup}|set(HAND+GRAVE+EXILE+ordinary.LIBRARY))
        for player in (0,1):
            checks += [step('assert_life',player=player,value=20)]+mana_receipts(player)
            for zone,names in zones[player].items():
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)]
                else:checks += [step('assert_zone_multiplicity',player=player,zone=zone,index=rows[n]['csvid'],value=Counter(names)[n]) for n in universe]
        post=[]
        if branch=='repeat_parent':
            legal=not a['disable_parent']
            post += [step('can_activate',ref='whelp',value=int(legal)),step('activate',ref='whelp',value=int(legal)),step('assert_stack',value=int(legal)),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
            if legal:pay(parent_owner,[0,0,0,0,1,0])
            post += [observe('whelp','power',2+int(legal)),observe('whelp','toughness',3),observe('whelp','plus1_counters',0)]+mana_receipts(parent_owner)
        elif branch=='expiry':
            post += [step('advance_turn'),observe('whelp','in_play',1),observe('whelp','power',2)]
            post += [step('set_mana',player=parent_owner,color=i,value=int(i==4)) for i in range(6)]
            post += [step('can_activate',ref='whelp',value=1),step('activate',ref='whelp',value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed'),observe('whelp','power',3),observe('whelp','toughness',3),step('assert_mana',player=parent_owner,color=4,value=0),step('cleanup'),observe('whelp','power',2),observe('whelp','in_play',1)]
        elif branch=='control_activation':
            post += [step('can_activate',ref='other_whelp',value=1),step('activate',ref='other_whelp',value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed'),observe('other_whelp','power',3),observe('other_whelp','toughness',3),observe('whelp','power',states['whelp']['power'])]
            pay(other_owner,[0,0,0,0,1,0]);post += mana_receipts(other_owner)
        steps=pre+actions+checks+post
        if full:steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
        case=dict(name=f'CounterActivatedRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=steps,
                  _recipe=dict(family='counterspell_extension',version=2,variant=variant,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=list(p['inactive_components'])))
        if owner:_swap_players(case)
        cases.append(case)
    return cases


# Resource-priced counter components: independent Oracle quantities and real payment/selection.
def _resource_counter_config(p):
    e=p.get('extension',{});q=e.get('resource_counter')
    if (set(e)!={'wave','resource_counter'} or e.get('wave')!=5 or not isinstance(q,dict)
            or set(q)!={'kind'} or q['kind'] not in ('reveal_blue','discard','delve','phyrexian_blue')
            or p['counter'] is not None or p['token'] is not None or p['relationship'] is not None or p['life_cost']):
        raise ValueError('reviewed resource-counter configuration required')
    return q


def _spec_resource_counter(d,p,o):
    from recipes import cost_from_oracle
    q=_resource_counter_config(p);mana=o['mana_cost']
    if q['kind']=='delve':
        if mana.count('{X}')!=1:raise ValueError('single printed delve X required')
        mana=mana.replace('{X}','')
    if q['kind']=='phyrexian_blue':
        if mana!='{U/P}':raise ValueError('reviewed blue Phyrexian symbol required')
        mana='{U}'
    p.update(family='counterspell_extension',mana=cost_from_oracle(dict(o,mana_cost=mana)))
    selected=d['variants'];available=_variants_resource_counter(p)
    if (any(not isinstance(v,str) for v in selected) or len(set(selected))!=len(selected)
            or any(v not in available for v in selected)):
        raise ValueError('unique reviewed resource-counter variants required')
    return p


def _variants_resource_counter(p):
    q=_resource_counter_config(p);base=['opponent','own_spell','payment','insufficient_mana','wrong_mana','no_stack','snapshot']
    if q['kind']=='phyrexian_blue':base += ['life_payment','life_snapshot','insufficient_life','wrong_value','sorcery_parent']
    else:base += ['resource_none','resource_one','resource_many','tax_paid','tax_exact','tax_unaffordable','instant_parent']
    if q['kind']=='delve':base += ['cash_only','delve_only']
    return [v+'_'+str(owner) for owner in (0,1) for v in base]


def _expand_resource_counter(name,csvid,d,p,selected,rows,o):
    from collections import Counter
    from recipes import card,step,observe,cost_from_oracle
    kind=_resource_counter_config(p)['kind'];available=_variants_resource_counter(p)
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected))!=len(selected) or any(v not in available for v in selected)):
        raise ValueError('unique reviewed resource-counter expansion required')
    results=[]
    for requested in selected:
        branch,owner=requested.rsplit('_',1);owner=int(owner);target=0 if branch=='own_spell' else 1
        full=branch in ('snapshot','life_snapshot');no_stack=branch=='no_stack';phy=kind=='phyrexian_blue'
        parent='Divination' if branch=='wrong_value' else 'Serum Visions' if branch=='sorcery_parent' else 'Giant Growth' if phy or branch=='instant_parent' else 'Grizzly Bears'
        life_payment=phy and branch in ('life_payment','life_snapshot','insufficient_life')
        count=(0 if branch=='resource_none' else 1 if branch=='resource_one' else 3 if branch=='resource_many' else 2 if kind=='reveal_blue' else 1)
        available_count=count
        # The real controller-one discard chooser stops at its AI cap of two.
        # Retain the third Forest as an unselected hand control.
        discard_ai_cap=kind=='discard' and branch=='resource_many' and owner==1
        if discard_ai_cap:count=2
        cash=2 if branch=='resource_many' else 0 if branch in ('resource_none','resource_one','delve_only') else 3 if branch=='cash_only' else 1
        exiled=0 if branch in ('resource_none','cash_only') else 1 if branch=='resource_one' else 3 if branch in ('resource_many','delve_only') else 2
        tax=cash+exiled if kind=='delve' else count*(3 if kind=='discard' else 1) if not phy else None
        paid=tax is not None and (tax==0 or branch in ('tax_paid','tax_exact'))
        failed=branch in ('insufficient_mana','wrong_mana','insufficient_life','wrong_value')
        cast_allowed=not failed and not no_stack;countered=cast_allowed and not paid
        source_cost=[0]*6 if life_payment else list(p['mana'])
        if kind=='delve':source_cost[0]+=cash
        setup=[card(name,'source',0,'hand'),card(parent,'spell',target,'hand'),card('Grizzly Bears','recipient',target),card('Mountain','hand_control',0,'hand')]
        setup += [card(n,f'library_{player}_{i}',player,'library') for player in (0,1) for i,n in enumerate(ordinary.LIBRARY)]
        if kind=='reveal_blue':setup += [card(('Wind Drake','Counterspell','Island')[i] if i<2 else 'Wind Drake',f'reveal_{i}',0,'hand') for i in range(count)]
        if kind=='discard':setup += [card('Forest',f'discard_{i}',0,'hand') for i in range(available_count)]
        if kind=='delve':setup += [card(n,f'delve_{i}',0,'graveyard') for i,n in enumerate(('Forest','Island','Mountain'))]
        if full:
            setup += [card('Plains',f'stock_hand_{player}',player,'hand') for player in (0,1)]
            setup += [card('Sol Ring',f'stock_grave_{player}',player,'graveyard') for player in (0,1)]
            setup += [card('Island',f'stock_exile_{player}',player,'exile') for player in (0,1)]
        pools=[[10]*6,[10]*6];life=[1 if branch=='insufficient_life' or phy and branch=='insufficient_mana' else 20,20];pre=[step('turn',player=target,value=0)]
        target_cost=cost_from_oracle(o[parent]);exact=branch in ('payment','life_payment','life_snapshot','insufficient_mana','wrong_mana','insufficient_life')
        if exact:
            pools[0]=[0]*6 if branch in ('insufficient_mana','insufficient_life') else list(source_cost)
            if branch=='wrong_mana':
                pools[0][3]+=pools[0][2];pools[0][2]=0
                # A Phyrexian card can still be paid with life: deny both ways.
                if phy:life[0]=1
            if target==0 and not no_stack:pools[0]=[a+b for a,b in zip(pools[0],target_cost)]
        if branch in ('tax_exact','tax_unaffordable'):
            pools[target]=list(target_cost);pools[target][0]+=tax if branch=='tax_exact' else 0
        if exact or branch in ('tax_exact','tax_unaffordable'):
            for player in (0,1):pre += [step('set_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
        if life[0]!=20:pre.append(step('set_life',player=0,value=life[0]))
        zones=[{z:[] for z in ('hand','battlefield','graveyard','exile','library')} for _ in (0,1)]
        for c in setup:zones[c['player']][c['zone']].append(c['name'])
        def move(player,n,origin,dest):zones[player][origin].remove(n);zones[player][dest].append(n)
        def mana_checks(player):return [step('assert_mana_total',player=player,value=sum(pools[player]))]+[step('assert_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
        actions=[]
        if not no_stack:
            if parent=='Giant Growth':actions.append(step('choose_target',player=target,ref='recipient'))
            actions += [step('cast',ref='spell',value=1),step('assert_stack',value=1)]
            pools[target]=[a-b for a,b in zip(pools[target],target_cost)]
            if cast_allowed and kind=='delve':
                actions += [step('choose_x',player=0,value=cash),step('choose_dialog',player=0,value=0 if exiled else 1)]
                if exiled:
                    actions += [step('choose_zone_card',player=0,value=rows[n]['csvid']) for n in ('Forest','Island','Mountain')[:exiled]]
                    if exiled<3+int(full):actions.append(step('choose_zone',player=0,value=-1))
            if cast_allowed and phy:actions.append(step('choose_number',player=0,value=int(life_payment)))
            actions += [step('cast_response',ref='source',value=int(cast_allowed)),step('assert_stack',value=2 if cast_allowed else 1)]
            if cast_allowed:
                pools[0]=[a-b for a,b in zip(pools[0],source_cost)]
                if life_payment:life[0]-=2
                actions += mana_checks(0)+[step('assert_life',player=0,value=life[0]),step('assert_zone_multiplicity',player=0,zone='hand',index=csvid,value=1),step('assert_zone_multiplicity',player=0,zone='graveyard',index=csvid,value=0)]
                if kind=='delve':
                    selected_grave=Counter(('Forest','Island','Mountain')[:exiled])
                    for n in sorted(set(zones[0]['graveyard']+zones[0]['exile'])):
                        actions += [step('assert_zone_multiplicity',player=0,zone='graveyard',index=rows[n]['csvid'],value=zones[0]['graveyard'].count(n)-selected_grave[n]),step('assert_zone_multiplicity',player=0,zone='exile',index=rows[n]['csvid'],value=zones[0]['exile'].count(n)+selected_grave[n])]
                if kind=='reveal_blue':
                    if owner==0:actions.append(step('choose_dialog',player=0,value=0))
                    if count:actions.append(step('choose_zone',player=owner,value=-1))
                if kind=='discard':
                    actions += [step('assert_zone_multiplicity',player=0,zone='hand',index=rows['Forest']['csvid'],value=available_count),step('assert_zone_multiplicity',player=0,zone='graveyard',index=rows['Forest']['csvid'],value=0)]
                    actions += [step('choose_zone_card',player=0,value=rows['Forest']['csvid']) for _ in range(count)]+([] if discard_ai_cap else [step('choose_zone',player=0,value=-1)]) if count else [step('choose_optional_zone',player=0,value=rows['Mountain']['csvid'])]
                if tax and branch!='tax_unaffordable':actions.append(step('choose_dialog',player=target,value=0 if paid else 1))
            actions += [step('resolve_response' if cast_allowed else 'resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
        else:actions += [step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
        if kind=='discard' and cast_allowed and not count:actions.append(step('assert_forced_zone_choices',value=0))
        if cast_allowed:
            move(0,name,'hand','graveyard')
            if kind=='discard':
                for _ in range(count):move(0,'Forest','hand','graveyard')
            if kind=='delve':
                for n in ('Forest','Island','Mountain')[:exiled]:move(0,n,'graveyard','exile')
            if paid and tax:pools[target][0]-=tax
        if not no_stack:
            move(target,parent,'hand','graveyard' if countered or 'Instant' in o[parent]['type_line'] or 'Sorcery' in o[parent]['type_line'] else 'battlefield')
            if not countered and parent=='Divination':
                for _ in range(2):zones[target]['hand'].append(zones[target]['library'].pop(0))
        checks=[step('assert_stack',value=0),step('assert_choices_consumed'),observe('recipient','power',5 if not no_stack and not countered and parent=='Giant Growth' else 2),observe('recipient','toughness',5 if not no_stack and not countered and parent=='Giant Growth' else 2),observe('recipient','plus1_counters',0),observe('recipient','minus1_counters',0)]
        universe={c['name'] for c in setup}
        for player in (0,1):
            checks += mana_checks(player)+[step('assert_life',player=player,value=life[player])]
            for z,names in zones[player].items():
                checks.append(step('assert_zone',player=player,zone=z,value=len(names)))
                checks += [step('assert_zone_multiplicity',player=player,zone=z,index=rows[n]['csvid'],value=Counter(names)[n]) for n in sorted(universe)]
                if z=='library':checks += [step('assert_zone_card',player=player,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)]
        steps=pre+actions+checks
        if full:steps=pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
        case=dict(name=f'CounterResourceRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='counterspell_extension',version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=list(p['inactive_components'])))
        if owner:_swap_players(case)
        results.append(case)
    return results


# Quiet actual-turn component: no castable post-resolution hand or battlefield.
def _delayed_counter_config(p):
    e=p.get('extension',{});q=e.get('delayed_counter')
    if (set(e)!={'wave','delayed_counter'} or e.get('wave')!=5
            or q!={'tax':1,'source_draw':1,'timing':'next_turn_upkeep'}
            or type(q['tax']) is not int or type(q['source_draw']) is not int
            or p['counter'] is not None or p['token'] is not None or p['relationship'] is not None or p['life_cost']):
        raise ValueError('reviewed delayed counter component required')
    return q


def _variants_delayed_counter(p):
    _delayed_counter_config(p)
    return [v+'_'+str(owner) for owner in (0,1) for v in ('opponent','own_spell','payment','insufficient_mana','wrong_mana','no_stack','snapshot','tax_paid','tax_exact','tax_unaffordable','delayed_once')]


def _spec_delayed_counter(d,p,o):
    from recipes import cost_from_oracle
    _delayed_counter_config(p);p.update(family='counterspell_extension',mana=cost_from_oracle(o))
    available=_variants_delayed_counter(p);selected=d['variants']
    if any(not isinstance(v,str) for v in selected) or len(set(selected))!=len(selected) or any(v not in available for v in selected):
        raise ValueError('unique reviewed delayed counter variants required')
    return p


def _expand_delayed_counter(name,csvid,d,p,selected,rows,o):
    from collections import Counter
    from recipes import card,step,cost_from_oracle
    _delayed_counter_config(p);available=_variants_delayed_counter(p)
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected))!=len(selected) or any(v not in available for v in selected)):
        raise ValueError('unique reviewed delayed counter expansion required')
    result=[]
    for requested in selected:
        branch,owner=requested.rsplit('_',1);owner=int(owner);target=0 if branch=='own_spell' else 1
        no_stack=branch=='no_stack';allowed=branch not in ('insufficient_mana','wrong_mana','no_stack');paid=branch in ('tax_paid','tax_exact');full=branch=='snapshot'
        setup=[card(name,'source',0,'hand'),card('Ancestral Recall','spell',target,'hand')]
        setup += [card('Counterspell',f'library_{player}_{i}',player,'library') for player in (0,1) for i in range(12)]
        if full:
            setup += [card('Counterspell',f'hand_control_{player}',player,'hand') for player in (0,1)]
            setup += [card('Forest',f'grave_control_{player}',player,'graveyard') for player in (0,1)]
            setup += [card('Island',f'exile_control_{player}',player,'exile') for player in (0,1)]
        # The empty-board advance occurs before any delayed effect and avoids
        # the first-game-turn draw skip. Thereafter ai_turns performs real
        # untap/upkeep/draw/main/cleanup; it explicitly runs player zero.
        pre=[step('advance_turn'),step('turn',player=target,value=0)]
        pools=[[0]*6,[0]*6];pools[0]=list(p['mana']) if branch!='insufficient_mana' else [0]*6
        if branch=='wrong_mana':pools[0][3]+=pools[0][2];pools[0][2]=0
        parent_cost=cost_from_oracle(o['Ancestral Recall'])
        if not no_stack:pools[target]=[a+b for a,b in zip(pools[target],parent_cost)];pools[target][0]+=int(branch!='tax_unaffordable')
        for player in (0,1):pre += [step('set_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
        zones=[{z:[] for z in ('hand','battlefield','graveyard','exile','library')} for _ in (0,1)]
        for c in setup:zones[c['player']][c['zone']].append(c['name'])
        def move(player,n,origin,dest):zones[player][origin].remove(n);zones[player][dest].append(n)
        def draw(player,count):
            for _ in range(count):zones[player]['hand'].append(zones[player]['library'].pop(0))
        def mana_checks(player):return [step('assert_mana_total',player=player,value=sum(pools[player]))]+[step('assert_mana',player=player,color=i,value=n) for i,n in enumerate(pools[player])]
        def zone_checks():
            checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
            for player in (0,1):
                checks += [step('assert_life',player=player,value=20)]+mana_checks(player)
                for z,names in zones[player].items():
                    checks.append(step('assert_zone',player=player,zone=z,value=len(names)))
                    checks += [step('assert_zone_multiplicity',player=player,zone=z,index=rows[n]['csvid'],value=Counter(names)[n]) for n in (name,'Ancestral Recall','Counterspell','Forest','Island')]
                    if z=='library':checks += [step('assert_zone_card',player=player,zone=z,index=i,value=rows[n]['csvid']) for i,n in enumerate(names)]
            return checks
        if no_stack:actions=[step('cast',ref='source',value=0),step('assert_stack',value=0),step('assert_choices_consumed')]
        else:
            actions=[step('choose_player',player=target,value=target),step('cast',ref='spell',value=1),step('assert_stack',value=1)]
            pools[target]=[a-b for a,b in zip(pools[target],parent_cost)]
            actions += [step('cast_response',ref='source',value=int(allowed)),step('assert_stack',value=2 if allowed else 1)]
            if allowed:
                pools[0]=[a-b for a,b in zip(pools[0],p['mana'])]
                actions += mana_checks(0)+[step('assert_zone_multiplicity',player=0,zone='hand',index=csvid,value=1),step('assert_zone_multiplicity',player=0,zone='graveyard',index=csvid,value=0)]
                if branch!='tax_unaffordable':actions.append(step('choose_dialog',player=target,value=0 if paid else 1))
            actions += [step('resolve_response' if allowed else 'resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
            if allowed:move(0,name,'hand','graveyard')
            move(target,'Ancestral Recall','hand','graveyard')
            if not allowed or paid:draw(target,3)
            if paid:pools[target][0]-=1
        actions += zone_checks()
        if not no_stack:
            # After optional parent Recall, every hand card is counter-only.
            # With an empty battlefield there is no incidental legal AI action.
            actions += [step('ai_turns',value=1)]
            draw(0,int(allowed)+int(owner==0));draw(1,int(owner==1))
            pools=[[0]*6,[0]*6];actions += zone_checks()
            if branch=='delayed_once':
                actions += [step('ai_turns',value=1)];draw(owner,1);actions += zone_checks()
        steps=pre+actions
        if full:steps=pre+[step('capture')]+copy.deepcopy(actions)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions)+[step('assert_recorded_state')]
        case=dict(name=f'CounterDelayedRecipe_{csvid}_{requested}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,_recipe=dict(family='counterspell_extension',version=2,variant=requested,params=copy.deepcopy(d['params']),oracle_text=d['oracle_text'],untested_components=list(p['inactive_components'])))
        if owner:_swap_players(case)
        result.append(case)
    return result
