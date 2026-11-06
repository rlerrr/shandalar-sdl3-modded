"""Explicit tap-for-mana components exercised through real activation and payment."""
import re
from creature_recipes import KEYWORDS, REMINDERS, BASE
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

SYMBOLS='CBUGRW'
ANY_COLOR=['{B}','{U}','{G}','{R}','{W}']
# Casting these witnesses exercises spending; their unused activated abilities
# do not supply expectations for the mana-producing creature.
WITNESSES={
    '{B}': ('Muck Rats','{B}',1,1,''),
    '{U}': ('Merfolk of the Pearl Trident','{U}',1,1,''),
    '{G}': ('Elvish Mystic','{G}',1,1,'{T}: Add {G}.'),
    '{R}': ("Mons's Goblin Raiders",'{R}',1,1,''),
    '{W}': ('Savannah Lions','{W}',2,1,''),
    '{G}{G}': ('Grizzly Bears','{1}{G}',2,2,''),
    '{C}{G}': ('Grizzly Bears','{1}{G}',2,2,''),
    '{R}{G}{W}': ('Woolly Thoctar','{R}{G}{W}',5,4,''),
}
HULK=('Phyrexian Hulk','{6}',5,4,'')
OUTPUT_VARIANTS=('production','payment','repeat_denied','opposing_turn','combat','spend','spend_generic','mana_snapshot','untap_repeat')


def output_pool(text):
    if not isinstance(text,str) or not re.fullmatch(r'(?:\{[CBUGRW]\}){1,3}',text):
        raise ValueError('mana outputs require one to three ordinary explicit symbols')
    return [text.count('{'+c+'}') for c in SYMBOLS]


def spec(declaration,record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout','normal')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or declaration.get('oracle_text')!=record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('mana component requires a pinned normal single-face creature and reviewed variants')
    p=declaration.get('params')
    if not isinstance(p,dict) or set(p)!={'keywords','activation_cost','mana_outputs','production'}:
        raise ValueError('explicit mana keywords, activation cost, outputs and production form required')
    outputs=p['mana_outputs'];form=p['production']
    if not isinstance(outputs,list) or not outputs or any(not isinstance(o,str) for o in outputs) or len(set(outputs))!=len(outputs):raise ValueError('unique explicit mana outputs required')
    pools=[output_pool(o) for o in outputs]
    if form=='fixed' and len(outputs)==1:wording=outputs[0]
    elif form=='choice' and 2<=len(outputs)<=3 and all(sum(o)==1 for o in pools):
        wording=' or '.join(outputs) if len(outputs)==2 else ', '.join(outputs[:-1])+', or '+outputs[-1]
    elif form=='any_color' and outputs==ANY_COLOR:wording='one mana of any color'
    else:raise ValueError('unreviewed mana production form')
    cost=p['activation_cost']
    if not isinstance(cost,str) or not re.fullmatch(r'(?:\{[1-3BUGRW]\}, )?\{T\}',cost):
        raise ValueError('mana component requires an ordinary optional input and tap cost')
    input_mana=cost_from_oracle(dict(mana_cost=cost.split(',')[0])) if ',' in cost else [0]*6
    clause=cost+': Add '+wording+'.'
    keys=[];abilities=[];canonical={k.lower():k for k in KEYWORDS+('Hexproof',)}
    for line in record['oracle_text'].splitlines():
        if ':' in line:abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed mana creature reminder')
            text=text.replace(reminder,'')
        tokens=[t.strip().lower() for t in text.strip().split(',')]
        if any(t not in canonical for t in tokens):raise ValueError('unreviewed mana creature passive or triggered clause')
        keys.extend(canonical[t] for t in tokens)
    if (abilities!=[clause] or p['keywords']!=sorted(keys) or len(set(keys))!=len(keys)
            or set(keys)!=set(record.get('keywords',[])) or set(keys)&{'Menace','Deathtouch'}):
        raise ValueError('mana clause and whole-card scope must match reviewed Oracle parameters')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')) or not 0<=int(record[field])<=20:raise ValueError('bounded numeric base stats required')
    if int(record['toughness'])==0:raise ValueError('mana creature must survive entry')
    if any(o not in WITNESSES and any(pool[1:]) for o,pool in zip(outputs,pools)):
        raise ValueError('mana output requires a reviewed colored-spell witness')
    return dict(family='mana_creature',keywords=sorted(keys),power=int(record['power']),toughness=int(record['toughness']),
                mana=cost_from_oracle(record),activation_mana=input_mana,outputs=outputs,output_pools=pools,choice=form!='fixed')


def variants(p):
    result=list(BASE)+['fresh_activation','sick_activation','tapped_activation']
    result += [v+'_'+str(i) for i in range(len(p['outputs'])) for v in OUTPUT_VARIANTS]
    if sum(p['activation_mana']):result.append('activation_insufficient_mana')
    if any(p['activation_mana'][1:]):result.append('activation_wrong_color')
    if not p['choice']:result.append('opposing_owner')
    if 'Vigilance' in p['keywords'] and 'Defender' not in p['keywords']:result.append('vigilance_attack')
    return result


def validate_witnesses(oracle,registry):
    for name,cost,power,toughness,text in list(WITNESSES.values())+[HULK]:
        r=oracle.get(name,{})
        if (name not in registry or r.get('layout')!='normal' or r.get('card_faces') or 'Creature' not in r.get('type_line','')
                or (r.get('mana_cost'),r.get('power'),r.get('toughness'),r.get('oracle_text'))!=(cost,str(power),str(toughness),text)):
            raise ValueError('mana spending witness changed: '+name)
    r=oracle.get('Vitalize',{})
    if 'Vitalize' not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=('{G}','Untap all creatures you control.'):
        raise ValueError('mana untapping witness changed')


def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,exact_mana,cost_from_oracle
    from creature_recipes import expand as creatures
    result=creatures(name,csvid,declaration,p,[v for v in selected if v in BASE],registry)
    for variant in (v for v in selected if v not in BASE):
        indexed=variant.rsplit('_',1)[-1].isdigit();kind,index=variant.rsplit('_',1) if indexed else (variant,'0');index=int(index)
        owner=int(kind=='opposing_owner');output=p['output_pools'][index];input_mana=p['activation_mana']
        setup=[card(name,'source',owner,zone='hand' if kind=='fresh_activation' else 'battlefield')]
        mana=[[10]*6,[10]*6];hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];battle=[0,0];battle[owner]=1
        pre=[];actions=[];extra=[];legal=True;keys=set(p['keywords'])
        if kind=='tapped_activation':setup[0]['state']=['tapped'];legal=False
        if kind=='sick_activation':setup[0]['state']=['sick'];legal='Haste' in keys
        if kind=='fresh_activation':
            pre += exact_mana(p['mana'])+[step('cast',ref='source',value=1),step('assert_stack',value=1),step('resolve'),step('assert_stack',value=0)]
            mana[owner]=list(input_mana);pre += exact_mana(input_mana);legal='Haste' in keys
        if kind=='payment':mana[owner]=list(input_mana);pre += exact_mana(input_mana)
        if kind=='activation_insufficient_mana':mana[owner]=[0]*6;pre += exact_mana(mana[owner]);legal=False
        if kind=='activation_wrong_color':
            mana[owner]=list(input_mana);c=next(i for i in range(1,6) if input_mana[i]);mana[owner][c]=0;mana[owner][1 if c!=1 else 2]=input_mana[c]
            pre += exact_mana(mana[owner]);legal=False
        if kind=='opposing_turn':pre.append(step('turn',player=1,value=0))
        if kind=='combat':
            setup.append(card('Grizzly Bears','attacker',1));battle[1]+=1
            pre += [step('turn',player=1,value=0),step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),
                    step('begin_blockers'),step('block',ref='source',other='attacker',value=1),step('advance_combat')]
            extra += [observe('source','blocking_count',1),observe('attacker','blocked',1),observe('attacker','in_play',1),observe('attacker','damage',0)]
        if kind=='vigilance_attack':
            pre += [step('begin_attackers'),step('attack',ref='source',value=1),observe('source','tapped',0),step('finish_attackers')]
            extra += [observe('source','attacking',1)]
        if kind=='untap_repeat':
            setup += [card('Vitalize','untap_spell',owner,zone='hand'),card('Grizzly Bears','ally',owner),card('Grizzly Bears','enemy',1-owner)]
            setup[-2]['state']=['tapped'];setup[-1]['state']=['tapped'];battle[0]+=1;battle[1]+=1
            extra += [observe('ally','tapped',0),observe('enemy','tapped',1)]
        if kind in ('spend','spend_generic'):
            witness=HULK if kind=='spend_generic' or not any(output[1:]) else WITNESSES[p['outputs'][index]]
            wname,wcost,wp,wt,_=witness;cost=cost_from_oracle(oracle[wname])
            # Fill only the shortfall: the new mana must be spent for casting to succeed.
            colored_excess=sum(max(0,output[c]-cost[c]) for c in range(1,6))
            assert output[0]+colored_excess<=cost[0], 'spending witness cannot consume this output'
            filler=[cost[0]-output[0]-colored_excess]+[max(0,cost[c]-output[c]) for c in range(1,6)]
            assert sum(filler)+sum(output)==sum(cost)
            mana[owner]=[input_mana[c]+filler[c] for c in range(6)];pre += exact_mana(mana[owner])
            setup.append(card(wname,'spend_spell',owner,zone='hand'))
            extra += [observe('spend_spell','in_play',1),observe('spend_spell','power',wp),observe('spend_spell','toughness',wt)]
        def activate():
            choices=[step('choose_color',player=owner,value=next(c for c in range(6) if output[c]))] if p['choice'] and legal else []
            return [step('can_activate',ref='source',value=int(legal))]+choices+[step('activate',ref='source',value=int(legal)),step('assert_stack',value=0),step('assert_choices_consumed')]
        actions += activate()
        if legal:mana[owner]=[mana[owner][c]-input_mana[c]+output[c] for c in range(6)]
        if kind=='repeat_denied':actions += [step('can_activate',ref='source',value=0),step('activate',ref='source',value=0),step('assert_stack',value=0)]
        if kind=='untap_repeat':
            actions += [observe('source','tapped',1),step('cast',ref='untap_spell',value=1),step('assert_stack',value=1),step('resolve'),observe('source','tapped',0)]
            mana[owner][3]-=1;graves[owner].append('Vitalize');actions += activate()
            mana[owner]=[mana[owner][c]-input_mana[c]+output[c] for c in range(6)]
        if kind in ('spend','spend_generic'):
            # Observe mana before payment as well as after it: payment alone cannot certify production.
            actions += [step('assert_mana',player=owner,color=c,value=mana[owner][c]) for c in range(6)]
            # Generic costs ask which colored pool to spend; script each pool click.
            actions += [step('choose_mana',player=owner,color=c) for c in range(1,6) for _ in range(max(0,output[c]-cost[c]))]
            # Vanilla creatures are cast by the core path; card_dummy has no EVENT_CAN_CAST result.
            actions += [step('cast',ref='spend_spell',value=1),step('assert_stack',value=1),step('resolve')]
            mana[owner]=[0]*6;battle[owner]+=1
        tapped=int(legal or kind=='tapped_activation')
        checks=[step('assert_stack',value=0),step('assert_choices_consumed'),observe('source','in_play',1),observe('source','tapped',tapped),
                observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',0)]+extra
        for player in (0,1):
            checks += [step('assert_life',player=player,value=20),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            checks += [step('assert_mana',player=player,color=c,value=mana[player][c]) for c in range(6)]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',LIBRARY),('exile',EXILE)):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                for i,n in enumerate(names) if zone=='library' else enumerate(dict.fromkeys(names)):
                    checks.append(step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) if zone=='library'
                                  else step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']))
        if kind=='mana_snapshot':steps=pre+[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+checks+[step('assert_recorded_state')]
        else:steps=pre+actions+checks
        result.append(dict(name=f'ManaCreatureRecipe_{csvid}_{variant}',card=name,fixture='stocked_zones',seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='mana_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_keywords=['Flash'] if 'Flash' in keys else [])))
    return result


# Wave-six opt-in branches keep every old declaration on its original path.
_wave6_original_spec = spec
_wave6_original_variants = variants
_wave6_original_expand = expand
_wave6_original_validate = validate_witnesses

def spec(d, o):
    from mana_spell_extension_recipes import applies, spec as extension_spec
    return extension_spec(d, o) if applies(d) else _wave6_original_spec(d, o)

def variants(p):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import variants as extension_variants
        return extension_variants(p)
    return _wave6_original_variants(p)

def validate_witnesses(o, registry):
    _wave6_original_validate(o, registry)

def expand(name, csvid, d, p, selected, registry, o):
    if p.get('wave') == 6:
        from mana_spell_extension_recipes import expand as extension_expand
        return extension_expand(name, csvid, d, p, selected, registry, o)
    return _wave6_original_expand(name, csvid, d, p, selected, registry, o)
