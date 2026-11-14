"""Fixed +1/+1 activations composed with real entry and death callbacks."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from permanent_value_activation_recipes import parse_cost, mana

FAMILY = 'permanent_plus1_activation'
DATA = Path(__file__).parent / 'data/permanent_plus1_activation_wave8_profiles.json'


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def parse(record, ability):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line', '').startswith('Token ')):
        raise ValueError('whole normal permanent required')
    lines = record['oracle_text'].splitlines()
    if type(ability) is not int or not 0 <= ability < len(lines) or ': ' not in lines[ability]:
        raise ValueError('explicit printed activated line required')
    raw_cost, effect = lines[ability].split(': ', 1)
    short = record['name'].split(',')[0]
    # Oracle's named self reference denotes the same source object; this is
    # semantic parsing, not a rewritten Oracle record or fabricated source.
    raw_cost = raw_cost.replace('from ' + short, 'from this creature')
    effect = effect.replace('on ' + short + '.', 'on this creature.')
    effects = {
        'Put a +1/+1 counter on this creature.': 'self',
        'Put a +1/+1 counter on target creature.': 'target',
        'Put a +1/+1 counter on target creature you control.': 'own_target',
        'Put a +1/+1 counter on each other creature you control.': 'others',
        'Put a +1/+1 counter on each creature you control and a loyalty counter on each other planeswalker you control.': 'mixed',
        'Put a +1/+1 counter on each of up to two target creatures.': 'upto_two',
        'Put a +1/+1 counter on up to one target creature.': 'upto_one',
        'Put a +1/+1 counter on each creature you control.': 'team',
    }
    if effect not in effects:
        raise ValueError('complete fixed counter effect required')
    cost = parse_cost(raw_cost)
    if cost['x'] or cost['discard'] or cost['pay_life'] or cost['sacrifice'] not in (None, 'artifact', 'land'):
        raise ValueError('unpiloted plus-one cost')
    if cost.get('counter', {}).get('kind', 'plus1') != 'plus1':
        raise ValueError('only printed +1/+1 counter costs supported')
    entry = 'none'; initial = 0
    if any(line.startswith('Modular 1 (') for line in lines): entry = 'fixed'; initial = 1
    if any('enters with X +1/+1 counters on it.' in line for line in lines): entry = 'x'
    if any(line.startswith('Converge — This creature enters with a +1/+1 counter') for line in lines): entry = 'colors'
    death = 'modular' if any(line.startswith('Modular 1 (') for line in lines) else None
    thopter_clause = ('When this creature dies, create a 1/1 colorless Thopter artifact creature token with flying '
                     'for each +1/+1 counter on this creature.')
    if thopter_clause in lines: death = 'thopters'
    growth = ('Whenever one or more +1/+1 counters are put on a creature you control, put that many growth counters on this enchantment.' in lines)
    creature = 'Creature' in record['type_line']; walker = 'Planeswalker' in record['type_line']
    if creature and any(not re.fullmatch(r'\d+', record.get(k, '')) for k in ('power', 'toughness')):
        raise ValueError('numeric printed creature stats required')
    if walker != ('loyalty' in cost): raise ValueError('printed planeswalker loyalty cost required')
    return dict(family=FAMILY, ability=ability, cost=cost, scope=effects[effect], creature=creature, walker=walker,
                initial_loyalty=int(record['loyalty']) if walker else 0, entry=entry, initial=initial, death=death,
                growth=growth, casting_x=record['mana_cost'].count('{X}'), activation_mana=mana(cost['mana']),
                land='Land' in record['type_line'],
                hexproof_team='You, planeswalkers you control, and other creatures you control have hexproof.' in lines,
                endstep_gap=any(line.startswith('At the beginning of your end step,') for line in lines),
                gaps=[line for i,line in enumerate(lines) if i != ability])


def spec(declaration, record):
    if declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2:
        raise ValueError('plus-one family and integer version2 required')
    params = declaration.get('params')
    if (not isinstance(params, dict) or set(params) != {'ability', 'interface', 'other_clauses'}
            or declaration.get('oracle_text') != (record or {}).get('oracle_text')):
        raise ValueError('complete Oracle and explicit ability/interface/gaps required')
    p = parse(record, params['ability']); source = pins()['sources'].get(record['name'], {})
    if (not source or any(record.get(k) != value for k,value in source['oracle'].items())
            or source['abilities'].get(str(p['ability'])) != params['interface']
            or params['other_clauses'] != p['gaps']):
        raise ValueError('independent whole source or interface differs')
    protocols = source.get('casting_protocols', {})
    if (not isinstance(protocols,dict) or any(not isinstance(k,str) or k not in variants(p) or not k.endswith('_0')
            or v != 'generic_placeholder_then_handler' for k,v in protocols.items())
            or protocols and (p['casting_x'] != 2 or p['entry'] != 'x' or record['mana_cost'] != '{X}{X}')):
        raise ValueError('reviewed double-X casting input protocol required')
    payments = source.get('casting_payment_repetitions', {})
    if (not isinstance(payments,dict) or any(not isinstance(k,str) or k not in variants(p)
            or type(v) is not int or v != 2 for k,v in payments.items())
            or payments and (p['entry'] != 'colors' or record['mana_cost'] != '{4}')):
        raise ValueError('measured duplicated Converge payment input required')
    ui = params['interface']
    if set(ui) != {'dialogs','sacrifice_choices','casting_colors','converge_colors','intent','unavailable'}:
        raise ValueError('complete scoped input interface required')
    if (not isinstance(ui['dialogs'],dict) or any(k not in ('0','1') or type(v) is not int or not 0 <= v <= 16 for k,v in ui['dialogs'].items())
            or not isinstance(ui['sacrifice_choices'],dict) or any(k not in ('0','1') or type(v) is not bool for k,v in ui['sacrifice_choices'].items())):
        raise ValueError('typed real modal/cost inputs required')
    if ui['converge_colors']:
        if (p['entry'] != 'colors' or ui['converge_colors'] != ['B','U','G','R'] or record['mana_cost'] != '{4}'):
            raise ValueError('four distinct real paid colors for printed generic4 required')
    if ui['casting_colors']:
        from defensive_creature_recipes import reviewed_payment
        reviewed_payment(record, {'payment_colors':ui['casting_colors']})
    if ui['intent'] is not None:
        intent = ui['intent']
        if (not isinstance(intent,dict) or set(intent) != {'mode_label','fallback_color','allow_auto_nonmana'}
                or intent['mode_label'] not in ('Add +1/+1 counter','Add +1/+1 counters') or type(intent['fallback_color']) is not int
                or not 0 <= intent['fallback_color'] <= 6 or intent['allow_auto_nonmana'] is not True
                or not (p['entry']=='colors' and any(': Add one mana of any color.' in s for s in p['gaps'])
                        or p['land'] and p['scope']=='team' and p['gaps']==['{T}: Add {C}.'])):
            raise ValueError('input intent scoped to independently reviewed unique counter/mana modes')
    if ui['unavailable'] and not (
            (p['entry']=='colors' or p['casting_x']==2 and p['death'] is None)
                and ui['unavailable']=={'0':'query_only','1':'disabled_dialog'}
            or p['land'] and ui['unavailable']=={'0':'query_only','1':'query_only'}):
        raise ValueError('actual human automatic-mana versus AI displayed-dialog policy required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(p))):
        raise ValueError('explicit distinct supported plus-one variants required')
    return p


def variants(p):
    if _follow5(p):return _follow5_variants(p)
    modes = ['value','payment','offturn','ready_snapshot','pending_snapshot','resolved_snapshot','cleanup','source_removed_pending']
    if p['creature']: modes += ['fresh','tapped']
    if not p['cost']['sacrifice']: modes += ['repeat']
    if p['cost']['tap']: modes += ['repeat_untap']
    if sum(p['activation_mana']): modes += ['insufficient_mana']
    if any(p['activation_mana'][1:]): modes += ['wrong_color']
    if p['scope'] in ('target','own_target','upto_two'):
        modes += ['target_enemy','target_removed_pending','probe_noncreature']
    if p['scope']=='target': modes += ['target_self']
    if p['scope']=='upto_two': modes += ['targets0','targets1']
    if p['entry']=='x': modes += ['entry_x0','entry_x1','entry_x3']
    if p['entry']=='colors': modes += ['converge_zero','converge_one']
    if 'counter' in p['cost']: modes += ['last_counter']
    if p['death']: modes += ['source_dies_after','death_snapshot']
    if p['death']=='modular': modes += ['death_decline']
    if p['death']=='thopters': modes += ['token_block_flying']
    return [mode+'_'+str(who) for who in (0,1) for mode in modes]


def validate_witnesses(oracle, registry):
    for nm,pin in pins()['witnesses'].items():
        if registry.get(nm,{}).get('csvid') != pin['csvid'] or any(oracle.get(nm,{}).get(k) != v for k,v in pin['oracle'].items()):
            raise ValueError('plus-one witness changed: '+nm)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    if _follow5(p):return _follow5_expand(name,csvid,declaration,p,selected,registry,oracle)
    from recipes import card, step
    from defensive_creature_recipes import reviewed_payment
    validate_witnesses(oracle, registry)
    out = []; cost = p['cost']; ui = declaration['params']['interface']
    for variant in selected:
        mode,owner_text=variant.rsplit('_',1);owner=int(owner_text);other=1-owner
        x={'entry_x0':0,'entry_x1':1,'entry_x3':3,'last_counter':1}.get(mode,2)
        setup=[];entities={};hands=[[],[]];graves=[[],[]];libs=[[],[]];exiles=[[],[]];actions=[];totals=[80,80];tokens=0

        def add(nm,ref,who,zone='battlefield'):
            if nm!=name and nm not in pins()['witnesses']:raise ValueError('unpinned plus-one entity: '+nm)
            item=card(nm,ref,who,zone)
            if zone=='battlefield':item['state']=['untapped']
            setup.append(item);r=oracle[nm]
            entities[ref]=dict(name=nm,player=who,alive=zone=='battlefield',tapped=False,plus1=0,loyalty=0,growth=0)
            if zone=='hand':hands[who].append(nm)

        def leave(ref):
            e=entities[ref];e['alive']=False;graves[e['player']].append(e['name'])

        def creature(ref):return 'Creature' in oracle[entities[ref]['name']]['type_line']

        def checks(stack=0):
            seq=[step('assert_stack',value=stack),step('assert_choices_consumed')];absent=set()
            for ref,e in entities.items():
                if not e['alive']:absent.add((e['player'],e['name']));continue
                seq += [step('require_card',ref=ref,property='in_play',value=1),step('require_card',ref=ref,property='tapped',value=int(e['tapped'])),
                        step('assert_counter',ref=ref,counter_kind='plus1',value=e['plus1'])]
                if creature(ref):seq.extend(step('require_card',ref=ref,property=k,value=int(oracle[e['name']][k])+e['plus1']) for k in ('power','toughness'))
                if 'Planeswalker' in oracle[e['name']]['type_line']:seq.append(step('assert_counter',ref=ref,counter_kind='loyalty',value=e['loyalty']))
                if ref=='source' and p['growth']:seq.append(step('assert_counter',ref=ref,counter_kind='growth',value=e['growth']))
            for who,nm in sorted(absent):seq.append(step('assert_zone_multiplicity',player=who,zone='battlefield',index=registry[nm]['csvid'],value=sum(e['alive'] and e['player']==who and e['name']==nm for e in entities.values())))
            if tokens:
                # Traits are copied from the complete Hangarback creator clause,
                # not the generic blue Thopter Oracle witness record.
                for prop,value in dict(token=1,color=0,artifact=1,subtype_Thopter=1,power=1,toughness=1).items():
                    seq.append(step('assert_matching_permanents',player=owner,index=registry['Thopter']['csvid'],property=prop,value=value))
                for i in range(tokens):seq += [step('bind_permanent_copy',player=owner,value=registry['Thopter']['csvid'],index=i,ref=f'token_{i}'),step('require_card',ref=f'token_{i}',property='tapped',value=0)]
            universe={e['name'] for e in entities.values()}|{'Plains','Forest','Island','Thopter'}
            for who in (0,1):
                seq += [step('assert_life',player=who,value=20),step('assert_mana_total',player=who,value=totals[who]),
                        step('assert_zone',player=who,zone='battlefield',value=sum(e['alive'] and e['player']==who for e in entities.values())+(tokens if who==owner else 0)),
                        step('assert_zone_multiplicity',player=who,zone='battlefield',index=registry['Thopter']['csvid'],value=tokens if who==owner else 0)]
                for zone,ns in (('hand',hands[who]),('graveyard',graves[who]),('library',libs[who]),('exile',exiles[who])):
                    seq.append(step('assert_zone',player=who,zone=zone,value=len(ns)))
                    if zone=='library':seq.extend(step('assert_zone_card',player=who,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(ns))
                    else:
                        counts=Counter(ns);seq.extend(step('assert_zone_multiplicity',player=who,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe))
            return seq

        # Resource ordering is an input fixture: the vanilla MV0 artifact is
        # available before any other artifact, so equal AI sacrifice values
        # retain the intended disposable resource without overriding AI choice.
        if cost['sacrifice']:add('Ornithopter','victim',owner)
        add('Coretapper','ally_artifact',owner);add('Sage of Lat-Nam','ally',owner)
        add('Coretapper','enemy_artifact',other);add('Sage of Lat-Nam','enemy',other)
        add('Icy Manipulator','invalid',owner)
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        if mode=='repeat_untap':add('Twiddle','untap',owner,'hand')
        if mode=='token_block_flying':add('Suntail Hawk','flier',other)
        if p['scope']=='mixed':
            add('Nissa, Voice of Zendikar','ally_walker',owner,'hand');add('Dovin, Grand Arbiter','enemy_walker',other,'hand')
        for who in (0,1):
            for zone,nm in (('hand','Plains'),('graveyard','Forest'),('library','Plains'),('exile','Island')):
                setup.append(card(nm,f'{zone}_{who}',who,zone));{'hand':hands,'graveyard':graves,'library':libs,'exile':exiles}[zone][who].append(nm)
        add(name,'source',owner,'hand')
        actions=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]

        def cast_permanent(ref,vector,chosen_x=None,colors=False):
            e=entities[ref];who=e['player']
            repetitions=pins()['sources'][name].get('casting_payment_repetitions',{}).get(variant,1) if ref=='source' else 1
            seq=[step('set_mana',player=who,color=c,value=v*repetitions) for c,v in enumerate(vector)]
            # An independently scoped legacy input annotation supplies the
            # generic dispatch placeholder. The subsequent handler receives
            # the declared game X; Oracle cost and entry calculations above
            # remain unchanged. The separate X=2,2 probe retains overcharge.
            if (ref=='source' and variant in pins()['sources'][name].get('casting_protocols',{})):
                seq.append(step('choose_x',player=who,value=0))
            if chosen_x is not None:seq.append(step('choose_x',player=who,value=chosen_x))
            if colors and who==0:seq.extend(step('choose_mana',player=who,color=c) for _payment in range(repetitions) for c in range(1,6) for _ in range(vector[c]))
            seq += [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=who,value=sum(vector)*(repetitions-1))]
            # Excess funding permits the actual second charge to finish, but
            # the correct one-payment receipt deliberately exposes that bug.
            if repetitions>1:seq.extend(step('assert_mana',player=who,color=c,value=v*(repetitions-1)) for c,v in enumerate(vector))
            hands[who].remove(e['name']);e['alive']=True;return seq

        if p['scope']=='mixed':
            actions.append(step('turn',player=other,value=0));actions+=cast_permanent('enemy_walker',mana(oracle['Dovin, Grand Arbiter']['mana_cost']))
            entities['enemy_walker']['loyalty']=3;actions += [step('assert_counter',ref='enemy_walker',counter_kind='loyalty',value=3),step('advance_turn')]
            actions+=cast_permanent('ally_walker',mana(oracle['Nissa, Voice of Zendikar']['mana_cost']));entities['ally_walker']['loyalty']=3
            actions.append(step('assert_counter',ref='ally_walker',counter_kind='loyalty',value=3))
        if p['entry']=='colors':
            vector=[4,0,0,0,0,0] if mode=='converge_zero' else [0,0,0,4,0,0] if mode=='converge_one' else [0,1,1,1,1,0]
            initial=sum(bool(v) for v in vector[1:])
        elif p['casting_x']:
            evaluated=oracle[name]['mana_cost'].replace('{X}','{'+str(x)+'}')
            # XX at chosen zero is a legal zero payment, even though the
            # general nonzero-cost fixture helper rejects the string {0}{0}.
            vector=[0]*6 if not evaluated.replace('{0}','') else mana(evaluated);initial=x
        elif ui['casting_colors']:
            vector=reviewed_payment(oracle[name],{'payment_colors':ui['casting_colors']})[0];initial=p['initial']
        else:vector=mana(oracle[name]['mana_cost']);initial=p['initial']
        actions+=cast_permanent('source',vector,x if p['casting_x'] else None,p['entry']=='colors')
        entities['source']['plus1']=initial;entities['source']['loyalty']=p['initial_loyalty']
        if p['creature'] and int(oracle[name]['toughness'])+initial<=0:leave('source')
        else:
            actions.append(step('assert_counter',ref='source',counter_kind='plus1',value=initial))
            if p['walker']:actions.append(step('assert_counter',ref='source',counter_kind='loyalty',value=p['initial_loyalty']))
        if p['creature'] and mode!='fresh' and entities['source']['alive']:actions += [step('advance_turn'),step('advance_turn')]
        pools=[[30,10,10,10,10,10] for _ in (0,1)]
        if mode=='payment':pools[owner]=list(p['activation_mana'])
        if mode=='insufficient_mana':pools[owner]=[0]*6
        if mode=='wrong_color':
            pools[owner]=list(p['activation_mana'])
            for c in range(1,6):pools[owner][0]+=pools[owner][c];pools[owner][c]=0
        totals=[sum(pool) for pool in pools];actions.extend(step('set_mana',player=who,color=c,value=v) for who in (0,1) for c,v in enumerate(pools[who]))
        if mode=='offturn':actions.append(step('turn',player=other,value=0))
        if mode=='tapped':
            actions += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            totals[other]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        actions.append(step('refresh'));ready=len(actions)
        targets=['ally'] if p['scope'] in ('target','own_target') else ['ally','enemy'] if p['scope']=='upto_two' else []
        if mode=='target_enemy':targets=['enemy']
        if mode=='target_self':targets=['source']
        if mode=='targets0':targets=[]
        if mode=='targets1':targets=['ally']
        legal=(entities['source']['alive'] and mode not in ('insufficient_mana','wrong_color')
               and not (p['walker'] and mode=='offturn') and not (cost['tap'] and mode in ('fresh','tapped')))
        if mode=='entry_x0':legal=False

        def deny():
            sibling=p['entry']=='colors' and entities['source']['plus1']>0 and mode in ('fresh','tapped','repeat')
            strategy=ui['unavailable'].get(str(owner)) if sibling else None
            seq=[step('require_can_activate',ref='source',value=int(sibling))]
            if strategy=='disabled_dialog':seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            if strategy!='query_only':seq.append(step('activate',ref='source',value=0))
            return seq

        def activate():
            seq=[step('require_can_activate',ref='source',value=1)]
            if ui['intent']:seq.append(step('choose_activation_mode',ref='source',player=owner,**ui['intent']))
            elif str(owner) in ui['dialogs']:seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            seq.extend(step('choose_target',player=owner,ref=ref) for ref in targets)
            if p['scope']=='upto_two' and len(targets)<2:seq.append(step('choose_cancel_target',player=owner))
            if cost['sacrifice'] and ui['sacrifice_choices'].get(str(owner),False):seq.append(step('choose_target',player=owner,ref='victim'))
            seq.append(step('activate',ref='source',value=1))
            if ui['intent']:seq.append(step('require_activation_mode',value=1))
            totals[owner]-=sum(p['activation_mana'])
            if cost['tap']:entities['source']['tapped']=True
            if cost['sacrifice']:leave('victim')
            if 'counter' in cost:
                entities['source']['plus1']-=cost['counter']['amount']
                if int(oracle[name]['toughness'])+entities['source']['plus1']<=0:
                    # The low-level activation API leaves ordinary effects
                    # pending. Recalculate SBAs before inspecting the source
                    # that became zero-toughness while paying its counter cost.
                    seq.append(step('refresh'));leave('source')
            if p['walker']:entities['source']['loyalty']+=cost['loyalty']
            return seq+checks(1)

        def effect():
            recipients=(['source'] if p['scope']=='self' else targets if p['scope'] in ('target','own_target','upto_two')
                        else [ref for ref,e in entities.items() if e['alive'] and e['player']==owner and creature(ref) and (p['scope']!='others' or ref!='source')])
            for ref in recipients:
                if entities[ref]['alive']:
                    entities[ref]['plus1']+=1
                    if p['growth'] and entities['source']['alive'] and entities[ref]['player']==owner:entities['source']['growth']+=1
            if p['scope']=='mixed':
                for ref,e in entities.items():
                    if ref!='source' and e['alive'] and e['player']==owner and 'Planeswalker' in oracle[e['name']]['type_line']:e['loyalty']+=1
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]

        def death(decline=False):
            nonlocal tokens
            seq=[];count=entities['source']['plus1']
            if p['death']=='modular':seq.append(step('choose_cancel_target',player=owner) if decline else step('choose_target',player=owner,ref='ally_artifact'))
            seq += [step('destroy',ref='source'),step('refresh'),step('resolve'),step('assert_choices_consumed')];leave('source')
            if p['death']=='modular' and not decline:entities['ally_artifact']['plus1']+=count
            if p['death']=='thopters':tokens+=count
            return seq

        probing=mode=='probe_noncreature' or mode=='target_enemy' and p['scope']=='own_target'
        if probing:
            actions.append(step('probe_activation_target',ref='source',other='invalid' if mode=='probe_noncreature' else 'enemy',index=ui['dialogs'].get(str(owner),-1),value=0))
        elif mode=='entry_x0':pass
        elif not legal:actions+=deny()
        else:
            actions+=activate();pending=len(actions)
            if mode=='source_removed_pending':
                if p['death']:actions+=death()
                else:actions += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='target_removed_pending':actions += [step('destroy',ref=targets[0]),step('refresh')];leave(targets[0])
            actions+=effect()
            if mode in ('source_dies_after','death_decline','death_snapshot','token_block_flying'):
                death_cut=len(actions);actions+=death(mode=='death_decline')
            if mode=='token_block_flying':
                actions += [step('bind_permanent_copy',player=owner,value=registry['Thopter']['csvid'],index=0,ref='combat_token'),
                            step('turn',player=other,value=0),step('begin_attackers'),step('attack',ref='flier',value=1),
                            step('finish_attackers'),step('begin_blockers'),step('block',ref='combat_token',other='flier',value=1),
                            step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
                leave('flier');tokens-=1
            if mode=='repeat':
                if cost['tap'] or p['walker']:actions+=deny()
                else:actions+=activate()+effect()
            if mode=='repeat_untap':
                hands[owner].remove('Twiddle');graves[owner].append('Twiddle');totals[owner]-=1;entities['source']['tapped']=False
                actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='untap',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]+activate()+effect()
            if mode=='cleanup':actions += [step('cleanup'),step('refresh')]
        final=checks();actions+=final
        if mode=='payment' and legal:actions.extend(step('assert_mana',player=owner,color=c,value=0) for c in range(6))
        if mode in ('ready_snapshot','pending_snapshot','death_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending if mode=='pending_snapshot' else death_cut;tail=actions[cut:]
            actions=actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot' and legal:actions += [step('capture'),step('record_state'),step('set_life',player=other,value=7),step('ai_roundtrip'),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        gaps=[s for s in p['gaps'] if not ('enters with' in s or s.startswith('Converge') or s.startswith('Modular') or s.startswith('When this creature dies, create') or s.startswith('Whenever one or more +1/+1 counters'))]
        out.append(dict(name=f'PermanentPlus1ActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                        _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        untested_components=gaps+['upkeep/win events are not synthesized','separate callback priority window','control changes','counter replacement effects'])))
    return out


def _follow5(p):
    return (p['land'] or p['scope'] in ('upto_one','team') or p['cost']['sacrifice']=='land'
            or p['casting_x']==2 and p['death'] is None)


def _follow5_variants(p):
    modes=['value','payment','offturn','ready_snapshot','pending_snapshot','resolved_snapshot','source_removed_pending']
    if not p['endstep_gap']:modes+=['cleanup']
    if p['creature']:modes+=['fresh','tapped']
    if p['land'] and p['cost']['tap']:modes+=['tapped']
    if not p['cost']['sacrifice']:modes+=['repeat']
    if p['cost']['tap']:modes+=['repeat_untap']
    if sum(p['activation_mana']):modes+=['insufficient_mana']
    if any(p['activation_mana'][1:]):modes+=['wrong_color']
    if p['scope']=='upto_one':modes+=['targets0','target_enemy','target_removed_pending','probe_noncreature']
    if p['scope']=='team':modes+=['no_other_creatures']
    if p['cost']['sacrifice']=='land':modes+=['no_land']
    if p['entry']=='x':modes+=['entry_x0','entry_x1','entry_x3']
    if p['hexproof_team']:modes+=['hexproof','flying_attack','flying_block','hexproof_flying']
    return [v+'_'+str(who) for who in (0,1) for v in modes]


def _follow5_expand(name,csvid,d,p,selected,registry,oracle):
    """Adjacent fixed effects, with input protocols distinct from Oracle costs."""
    from recipes import card,step
    validate_witnesses(oracle,registry)
    result=[];cost=p['cost'];ui=d['params']['interface'];source_pin=pins()['sources'][name]
    for variant in selected:
        mode,who=variant.rsplit('_',1);owner=int(who);other=1-owner
        x={'entry_x0':0,'entry_x1':1,'entry_x3':3}.get(mode,2)
        setup=[];entities={};zones={z:[[],[]] for z in ('hand','graveyard','library','exile')};actions=[]
        pools=[[30,10,10,10,10,10] for _ in (0,1)];life=[20,20]

        def add(nm,key,pl,zone='battlefield'):
            if nm!=name and nm not in pins()['witnesses']:raise ValueError('unpinned follow-on witness '+nm)
            c=card(nm,key,pl,zone)
            if zone=='battlefield':c['state']=['untapped']
            setup.append(c);entities[key]=dict(name=nm,player=pl,alive=zone=='battlefield',tapped=False,plus1=0,loyalty=0,damage=0,sick=False)
            if zone!='battlefield':zones[zone][pl].append(nm)

        def leave(key,zone='graveyard'):
            e=entities[key];e['alive']=False;zones[zone][e['player']].append(e['name'])

        def creature(key):return 'Creature' in oracle[entities[key]['name']]['type_line']

        def target_checks():
            if not p['hexproof_team']:return []
            alive=entities['source']['alive'];seq=[]
            if 'ally' in entities:
                seq += [step('can_target_creature',ref='enemy_rod',other='ally',value=int(not alive)),
                        step('probe_activation_target',ref='enemy_rod',other='ally',index=-1,value=int(not alive)),
                        step('can_target_creature',ref='own_rod',other='ally',value=1)]
            seq += [step('can_target_creature',ref='own_rod',other='enemy',value=1),
                    step('probe_activation_target',ref='enemy_rod',other='ally_walker',index=-1,value=int(not alive)),
                    step('can_cast',ref='duress',value=int(not alive))]
            if alive:seq += [step('can_target_creature',ref='enemy_rod',other='source',value=1),step('probe_activation_target',ref='enemy_rod',other='source',index=-1,value=1)]
            return seq

        def checks(stack=0):
            seq=[step('assert_stack',value=stack),step('assert_choices_consumed')]
            for key,e in entities.items():
                if not e['alive']:continue
                seq += [step('require_card',ref=key,property='in_play',value=1),step('require_card',ref=key,property='tapped',value=int(e['tapped'])),
                        step('assert_counter',ref=key,counter_kind='plus1',value=e['plus1'])]
                if creature(key):seq += [step('require_card',ref=key,property=k,value=int(oracle[e['name']][k])+e['plus1']) for k in ('power','toughness')]+[step('assert_card',ref=key,property='damage',value=e['damage'])]
                if key=='source' and p['creature']:seq.append(step('require_card',ref=key,property='sick',value=int(e['sick'])))
                if 'Planeswalker' in oracle[e['name']]['type_line']:seq.append(step('assert_counter',ref=key,counter_kind='loyalty',value=e['loyalty']))
            names={e['name'] for e in entities.values()}|{'Forest','Plains','Island'}
            for pl in (0,1):
                seq += [step('assert_life',player=pl,value=life[pl]),step('assert_mana_total',player=pl,value=sum(pools[pl])),
                        step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
                seq += [step('assert_zone_multiplicity',player=pl,zone='battlefield',index=registry[nm]['csvid'],value=sum(e['alive'] and e['player']==pl and e['name']==nm for e in entities.values())) for nm in sorted(names)]
                for zone in ('hand','graveyard','library','exile'):
                    ns=zones[zone][pl];seq.append(step('assert_zone',player=pl,zone=zone,value=len(ns)))
                    if zone=='library':seq += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(ns)]
                    else:
                        counts=Counter(ns);seq += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(names)]
            return seq+target_checks()

        if cost['sacrifice']=='land':
            if mode!='no_land':add('Plains','victim',owner)
            add('Island','enemy_land',other)
        if mode!='no_other_creatures':add('Coretapper','ally_artifact',owner);add('Sage of Lat-Nam','ally',owner)
        add('Coretapper','enemy_artifact',other);add('Sage of Lat-Nam','enemy',other);add('Icy Manipulator','invalid',owner)
        if mode=='tapped':add('Icy Manipulator','tapper',other)
        if mode=='repeat_untap':add('Twiddle','untap',owner,'hand')
        if p['scope']=='team':add('Nissa, Voice of Zendikar','ally_walker',owner,'hand')
        if p['hexproof_team']:
            add('Rod of Ruin','own_rod',owner);add('Rod of Ruin','enemy_rod',other);add('Duress','duress',other,'hand')
            if mode in ('hexproof','hexproof_flying'):add('Tome Scour','self_spell',owner,'hand')
            if mode in ('flying_block','hexproof_flying'):add('Suntail Hawk','flier',other)
        for pl in (0,1):
            for zone,nm in (('hand','Plains'),('graveyard','Forest'),('library','Plains'),('exile','Island')):
                setup.append(card(nm,f'{zone}_{pl}',pl,zone));zones[zone][pl].append(nm)
        add(name,'source',owner,'battlefield' if p['land'] else 'hand')
        actions=[step('turn',player=owner,value=0),step('present_ai_dialogs',value=1)]

        def fund(pl,vector):return [step('set_mana',player=pl,color=c,value=v) for c,v in enumerate(vector)]

        def cast(key,vector,chosen_x=None):
            pl=entities[key]['player'];seq=fund(pl,vector)
            if chosen_x is not None:
                if variant in source_pin.get('casting_protocols',{}):seq.append(step('choose_x',player=pl,value=0))
                seq.append(step('choose_x',player=pl,value=chosen_x))
            seq += [step('cast',ref=key,value=1),step('assert_stack',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed'),step('assert_mana_total',player=pl,value=0)]
            zones['hand'][pl].remove(entities[key]['name']);entities[key]['alive']=True;entities[key]['sick']=creature(key);return seq

        if p['scope']=='team':
            actions+=cast('ally_walker',mana(oracle['Nissa, Voice of Zendikar']['mana_cost']));entities['ally_walker']['loyalty']=3
        if not p['land']:
            vector=[2*x,0,0,0,0,0] if p['casting_x']==2 else mana(oracle[name]['mana_cost'])
            actions+=cast('source',vector,x if p['casting_x'] else None)
        entities['source']['plus1']=x if p['entry']=='x' else p['initial'];entities['source']['loyalty']=p['initial_loyalty']
        if p['creature'] and int(oracle[name]['toughness'])+entities['source']['plus1']<=0:leave('source')
        else:
            actions.append(step('assert_counter',ref='source',counter_kind='plus1',value=entities['source']['plus1']))
            if p['walker']:actions.append(step('assert_counter',ref='source',counter_kind='loyalty',value=p['initial_loyalty']))
        # Non-T Razerunners is legal directly after its real cast. Advancing
        # turns would also dispatch its deliberately unselected end-step clause.
        if p['creature'] and not p['endstep_gap'] and mode!='fresh' and entities['source']['alive']:
            actions += [step('advance_turn'),step('advance_turn')];entities['source']['sick']=False
        if mode=='payment':pools[owner]=list(p['activation_mana'])
        if mode=='insufficient_mana':pools[owner]=[0]*6
        if mode=='wrong_color':pools[owner]=[sum(p['activation_mana']),0,0,0,0,0]
        actions += fund(0,pools[0])+fund(1,pools[1])
        if mode=='offturn':actions.append(step('turn',player=other,value=0))
        if mode=='tapped':
            actions += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            pools[other][0]-=1;entities['tapper']['tapped']=True;entities['source']['tapped']=True
        actions.append(step('refresh'));actions+=target_checks();ready=len(actions)
        targets=[] if p['scope']!='upto_one' or mode=='targets0' else ['enemy' if mode=='target_enemy' else 'ally']
        legal=(entities['source']['alive'] and mode not in ('insufficient_mana','wrong_color','no_land')
               and not (p['walker'] and mode=='offturn') and not (cost['tap'] and mode=='tapped'))

        def deny():
            sibling=(p['land'] and not entities['source']['tapped'] or p['casting_x']==2 and entities['source']['alive'] and entities['source']['plus1']>0)
            seq=[step('require_can_activate',ref='source',value=int(sibling))]
            strategy=ui['unavailable'].get(str(owner)) if sibling else None
            if strategy=='disabled_dialog':seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            if strategy!='query_only':seq.append(step('activate',ref='source',value=0))
            return seq

        def activate():
            seq=[step('require_can_activate',ref='source',value=1)]
            if ui['intent']:seq.append(step('choose_activation_mode',ref='source',player=owner,**ui['intent']))
            elif str(owner) in ui['dialogs']:seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            seq += [step('choose_target',player=owner,ref=k) for k in targets]
            if p['scope']=='upto_one' and not targets:seq.append(step('choose_optional_target',player=owner,ref='ally'))
            if cost['sacrifice'] and ui['sacrifice_choices'].get(str(owner),False):seq.append(step('choose_target',player=owner,ref='victim'))
            seq.append(step('activate',ref='source',value=1))
            if ui['intent']:seq.append(step('require_activation_mode',value=1))
            if p['scope']=='upto_one' and not targets:seq.append(step('assert_forced_target_choices',value=0))
            pools[owner]=[a-b for a,b in zip(pools[owner],p['activation_mana'])]
            if cost['tap']:entities['source']['tapped']=True
            if cost['sacrifice']:leave('victim')
            if p['walker']:entities['source']['loyalty']+=cost['loyalty']
            return seq+checks(1)

        def effect():
            recipients=['source'] if p['scope']=='self' else targets if p['scope']=='upto_one' else [k for k,e in entities.items() if e['alive'] and e['player']==owner and creature(k)]
            for k in recipients:
                if entities[k]['alive']:entities[k]['plus1']+=1
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]

        if mode=='entry_x0':pass
        elif mode=='probe_noncreature':actions.append(step('probe_activation_target',ref='source',other='invalid',index=ui['dialogs'][str(owner)],value=0))
        elif not legal:actions+=deny()
        else:
            actions+=activate();pending=len(actions)
            if mode=='source_removed_pending':actions += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='target_removed_pending':actions += [step('destroy',ref=targets[0]),step('refresh')];leave(targets[0])
            actions+=effect()
            if mode=='repeat':
                if cost['tap'] or p['walker']:actions+=deny()
                else:actions+=activate()+effect()
            if mode=='repeat_untap':
                actions += [step('choose_target',player=owner,ref='source'),step('cast',ref='untap',value=1),step('choose_dialog',player=owner,value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
                zones['hand'][owner].remove('Twiddle');zones['graveyard'][owner].append('Twiddle');pools[owner][2]-=1;entities['source']['tapped']=False
                actions+=activate()+effect()
            if mode in ('hexproof','hexproof_flying'):
                actions += [step('probe_activation_target',ref='own_rod',other='ally_walker',index=-1,value=1),
                            step('choose_player',player=owner,value=owner),step('cast',ref='self_spell',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
                zones['hand'][owner].remove('Tome Scour');zones['graveyard'][owner]+=['Tome Scour']+zones['library'][owner];zones['library'][owner]=[];pools[owner][2]-=1
                actions+=checks()
                if mode=='hexproof':actions += [step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='flying_attack':
                actions += [step('begin_attackers'),step('attack',ref='source',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='enemy_artifact',other='source',value=0),step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
                entities['source']['tapped']=True;life[other]-=int(oracle[name]['power'])+entities['source']['plus1']
            if mode in ('flying_block','hexproof_flying'):
                actions += [step('turn',player=other,value=0),step('begin_attackers'),step('attack',ref='flier',value=1),step('finish_attackers'),step('begin_blockers'),step('block',ref='source',other='flier',value=1),step('advance_combat'),step('combat_damage'),step('resolve'),step('assert_choices_consumed'),step('end_combat')]
                leave('flier');entities['source']['damage']=1
            if mode=='hexproof_flying':actions+=checks()+[step('destroy',ref='source'),step('refresh')];leave('source')
            if mode=='cleanup':actions += [step('cleanup'),step('refresh')]
        final=checks();actions+=final
        if mode=='payment' and legal:actions += [step('assert_mana',player=owner,color=c,value=0) for c in range(6)]
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending;tail=actions[cut:]
            actions=actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot' and legal:actions += [step('capture'),step('record_state'),step('set_life',player=other,value=7),step('ai_roundtrip'),step('restore')]+copy.deepcopy(final)+[step('assert_recorded_state')]
        composed=[s for s in p['gaps'] if 'enters with' not in s and not (p['hexproof_team'] and s in ('Flying','You, planeswalkers you control, and other creatures you control have hexproof.'))]
        if 'query_only' in ui['unavailable'].values():composed.append('selected unavailable mode is not attempted when the actual interface offers only a different action; global sibling permission remains asserted')
        result.append(dict(name=f'PermanentPlus1ActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=composed+['separate callback priority window','control changes','counter replacement effects'])))
    return result
