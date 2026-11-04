"""Explicit self-pump components with real activation, costs, stack and combat."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from grant_recipes import HAND, LIBRARY, GRAVE, EXILE


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout','normal')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or declaration.get('oracle_text')!=record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('self-pump requires a pinned normal single-face creature and reviewed variants')
    canonical={k.lower():k for k in KEYWORDS};keys=[];abilities=[]
    # This scope check rejects passive/triggered clauses. Pump amounts and
    # ability cost are explicit reviewed parameters, never taken from handlers.
    for line in record['oracle_text'].splitlines():
        if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line) and ':' in line:
            abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS:raise ValueError('unreviewed unconditional keyword reminder')
            text=text.replace(reminder,'')
        tokens=[t.strip().lower() for t in text.strip().split(',')]
        if any(t not in canonical for t in tokens):raise ValueError('self-pump has unreviewed passive or triggered rules')
        keys.extend(canonical[t] for t in tokens)
    p=declaration.get('params')
    if not isinstance(p,dict) or not {'keywords','power','toughness','activation_cost'} <= set(p) or set(p)-{'keywords','power','toughness','activation_cost','activation_limit'}:
        raise ValueError('unknown or missing self-pump parameters')
    if p['keywords']!=sorted(keys) or len(set(keys))!=len(keys) or not abilities:
        raise ValueError('self-pump unconditional keywords must match the reviewed keyword clauses')
    if any(type(p[k]) is not int or not 0<=p[k]<=4 for k in ('power','toughness')) or not p['power']+p['toughness']:
        raise ValueError('self-pump requires a bounded nonzero nonnegative bonus')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')) or not 0<=int(record[field])<=20:
            raise ValueError('self-pump needs bounded numeric Oracle base characteristics')
    if int(record['toughness'])==0:raise ValueError('self-pump host must survive entry')
    if 'activation_limit' in p and (type(p['activation_limit']) is not int or not 2<=p['activation_limit']<=3):
        raise ValueError('self-pump requires a reviewed per-turn limit of two or three')
    if not isinstance(p['activation_cost'],str):raise ValueError('self-pump requires a structured activation cost')
    reviewed=f"{p['activation_cost']}: This creature gets +{p['power']}/+{p['toughness']} until end of turn."
    if p.get('activation_limit'):reviewed += ' Activate no more than '+{2:'twice',3:'three times'}[p['activation_limit']]+' each turn.'
    if reviewed not in abilities:
        raise ValueError('self-pump clause differs from its explicitly reviewed cost, bonus, or limit')
    dialog=declaration.get('activation_dialog')
    if 'activation_dialog' in declaration and (type(dialog) is not int or not 0<=dialog<=8):
        raise ValueError('activation_dialog requires a bounded zero-based menu choice')
    dialogs=declaration.get('activation_dialogs',{})
    if (not isinstance(dialogs,dict) or any(k not in declaration['variants'] or type(v) is not int or not 0<=v<=8 for k,v in dialogs.items())):
        raise ValueError('activation_dialogs requires selected variants and bounded zero-based menu choices')
    cancellations=declaration.get('activation_cancel_mana',[])
    if (not isinstance(cancellations,list) or any(not isinstance(v,str) for v in cancellations) or len(set(cancellations))!=len(cancellations)
            or any(v!='activation_payment' or v not in declaration['variants'] for v in cancellations)):
        raise ValueError('activation_cancel_mana requires a reviewed exact-payment variant')
    cost=cost_from_oracle(dict(mana_cost=p['activation_cost']))
    return dict(family='self_pump_creature',keywords=p['keywords'],power=int(record['power']),toughness=int(record['toughness']),
                mana=cost_from_oracle(record),boost_power=p['power'],boost_toughness=p['toughness'],activation_mana=cost,
                limit=p.get('activation_limit'),other_abilities=len(abilities)>1,remaining_abilities=abilities)


def variants(p):
    from creature_recipes import variants as creature_variants
    result=creature_variants(p)+['activation','activation_payment','stacked_activations','repeated_activations',
            'tapped_activation','fresh_activation','opposing_turn_activation','combat_activation','lost_source','activation_snapshot']
    if not p['other_abilities']:
        result.append('activation_insufficient_mana')
        if any(p['activation_mana'][1:]):result.append('activation_wrong_color')
    if p['limit']:result += ['activation_limit','activation_limit_reset']
    else:result.append('five_activations')
    return result


def validate_witnesses(oracle,registry):
    expected={'Unsummon':('{U}',"Return target creature to its owner's hand.")}
    for name,(cost,text) in expected.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=(cost,text):
            raise ValueError('self-pump response witness changed: '+name)
    for name in ('Hill Giant','Grizzly Bears'):
        r=oracle.get(name,{})
        pt=('3','3') if name=='Hill Giant' else ('2','2')
        if name not in registry or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=(*pt,''):
            raise ValueError('self-pump unselected witness changed: '+name)


def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,exact_mana
    from creature_recipes import expand as creatures,variants as creature_variants
    base=set(creature_variants(p));result=creatures(name,csvid,declaration,p,[v for v in selected if v in base],registry)
    for variant in (v for v in selected if v not in base):
        setup=[card(name,'source',zone='hand' if variant=='fresh_activation' else 'battlefield'),
               card('Hill Giant','unselected'),card('Grizzly Bears','enemy_unselected',1)]
        if variant=='tapped_activation':setup[0]['state']=['tapped']
        lost=variant=='lost_source';fresh=variant=='fresh_activation';combat=variant=='combat_activation'
        hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];mana=[60,60];life=[20,20]
        if lost:setup.append(card('Unsummon','response',zone='hand'));hands[0].append('Unsummon')
        pre=[];power=p['power'];toughness=p['toughness'];boost=p['boost_power'];boost_t=p['boost_toughness']
        if fresh:
            # This field exposes the engine's effective sickness restriction;
            # haste clears it while the fixture still exercises fresh entry.
            pre += [step('cast',ref='source',value=1),step('resolve'),step('assert_choices_consumed'),observe('source','sick',int('Haste' not in p['keywords']))]
            mana[0]-=sum(p['mana'])
        if variant=='opposing_turn_activation':pre.append(step('turn',player=1,value=0))
        if combat:
            allowed=int('Defender' not in p['keywords'])
            pre += [step('begin_attackers'),step('attack',ref='source',value=allowed),step('finish_attackers')]
        negative=variant in ('activation_insufficient_mana','activation_wrong_color')
        if variant=='activation_payment':pre+=exact_mana(p['activation_mana']);mana[0]=sum(p['activation_mana'])
        if negative:
            pools=[0]*6
            if variant=='activation_wrong_color':pools[0]=sum(p['activation_mana'])
            pre+=exact_mana(pools);mana[0]=sum(pools)
        repeat=(p['limit'] or 3) if variant=='repeated_activations' else 2 if variant=='stacked_activations' else 5 if variant=='five_activations' else p['limit'] if variant in ('activation_limit','activation_limit_reset') else 1
        if not negative and repeat*p['activation_mana'][0]>10:
            reserve=repeat*p['activation_mana'][0]
            pre.append(step('set_mana',player=0,color=0,value=reserve));mana[0]+=reserve-10
        actions=[]
        def activation():
            s=[step('can_activate',ref='source',value=1)]
            dialog=declaration.get('activation_dialogs',{}).get(variant,declaration.get('activation_dialog'))
            if dialog is not None:s.append(step('choose_dialog',player=0,value=dialog))
            if variant in declaration.get('activation_cancel_mana',[]):s.append(step('choose_cancel_mana',player=0))
            return s+[step('activate',ref='source',value=1)]
        if negative:
            actions += [step('can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
        else:
            pending=variant=='stacked_activations'
            for n in range(repeat):
                actions+=activation();mana[0]-=sum(p['activation_mana'])
                if lost:actions += [step('choose_priority',ref='response'),step('choose_target',player=-1,ref='source')]
                actions += [step('assert_stack',value=n+1 if pending else 1),
                            observe('source','power',power),observe('source','toughness',toughness),
                            step('assert_mana_total',player=0,value=mana[0])]
                if not pending:
                    actions += [step('resolve'),step('assert_choices_consumed')]
                    if not lost:
                        power+=boost;toughness+=boost_t
                        actions += [observe('source','power',power),observe('source','toughness',toughness)]
            if pending:
                actions += [step('resolve'),step('assert_choices_consumed')];power+=repeat*boost;toughness+=repeat*boost_t
                actions += [observe('source','power',power),observe('source','toughness',toughness)]
            if p['limit'] and variant in ('activation_limit','activation_limit_reset'):
                actions.append(step('can_activate',ref='source',value=0))
        if lost:
            hands[0].remove('Unsummon');hands[0].append(name);graves[0].append('Unsummon');mana[0]-=1
            actions += [observe('source','in_play',0)]
        if combat:
            allowed=int('Defender' not in p['keywords']);total=power*(2 if 'Double strike' in p['keywords'] else 1) if allowed else 0
            actions += [step('begin_blockers'),step('advance_combat'),step('combat_damage')]
            life[1]-=total
            if 'Lifelink' in p['keywords']:life[0]+=total
            actions += [step('assert_life',player=i,value=life[i]) for i in (0,1)]
        actions += [observe('unselected','power',3),observe('unselected','toughness',3),observe('enemy_unselected','power',2),observe('enemy_unselected','toughness',2)]
        # Snapshot records a resolved dynamic effect, then replays from before
        # activation. Fixture snapshots are caller owned and survive AI backups.
        resolved_checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            resolved_checks += [step('assert_mana_total',player=player,value=mana[player]),step('assert_life',player=player,value=life[player]),
                step('assert_zone',player=player,zone='battlefield',value=(1 if lost else 2) if player==0 else 1)]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',LIBRARY),('exile',EXILE)):
                resolved_checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                if zone=='library':resolved_checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) for i,n in enumerate(names)]
                else:resolved_checks += [step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']) for n in Counter(names)]
        cleanup=[] if lost else [step('cleanup'),observe('source','in_play',1),observe('source','power',p['power']),observe('source','toughness',p['toughness']),
            observe('source','tapped',int(variant=='tapped_activation' or combat and 'Defender' not in p['keywords'] and 'Vigilance' not in p['keywords']))]
        if variant=='activation_limit_reset':
            cleanup += activation()+[step('resolve'),step('assert_choices_consumed'),observe('source','power',p['power']+boost),observe('source','toughness',p['toughness']+boost_t),
                                     step('assert_mana_total',player=0,value=mana[0]-sum(p['activation_mana']))]
        if variant=='activation_snapshot':
            steps=pre+[step('capture')]+actions+resolved_checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+resolved_checks+[step('assert_recorded_state')]+cleanup
        else:steps=pre+actions+resolved_checks+cleanup
        result.append(dict(name=f'PumpCreatureRecipe_{csvid}_{variant}',card=name,fixture='stocked_zones',seed=123,setup=setup,steps=steps,
            _recipe=dict(family='self_pump_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                         **({'activation_dialog':declaration.get('activation_dialogs',{}).get(variant,declaration.get('activation_dialog'))} if 'activation_dialog' in declaration or variant in declaration.get('activation_dialogs',{}) else {}),
                         **({'activation_cancel_mana':True} if variant in declaration.get('activation_cancel_mana',[]) else {}))))
    return result
