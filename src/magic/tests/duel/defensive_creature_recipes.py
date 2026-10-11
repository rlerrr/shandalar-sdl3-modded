"""Reviewed defensive components; real targeting, combat, and untargeted removal."""
import re
from creature_recipes import KEYWORDS, REMINDERS, BASE
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

SHIELDS = ('Hexproof', 'Shroud', 'Indestructible')
PROTECTION = ('white','blue','black','red','green','creatures','artifacts','monocolored','multicolored')
EXTRA_REMINDERS = {
    "(This creature can't be the target of spells or abilities your opponents control.)",
    "(This creature can't be the target of spells or abilities.)",
    '(Damage and effects that say "destroy" don\'t destroy this creature.)',
    '(Damage and effects that say "destroy" don\'t destroy this creature. If its toughness is 0 or less, it still dies.)',
    '(Damage and effects that say "destroy" don\'t destroy this creature. If its toughness is 0 or less, it\'s still put into its owner\'s graveyard.)',
} | {f"(This creature can't be blocked, targeted, dealt damage, or enchanted by anything {c}.)" for c in ('white','blue','black','red','green')}

def protection_values(text):
    """Exact reviewed color-list grammar; subtype and conditional lists fail closed."""
    if text in PROTECTION:return [text]
    colors=('white','blue','black','red','green')
    for count in (2,3):
        import itertools
        for selected in itertools.permutations(colors,count):
            phrase=(selected[0]+' and from '+selected[1]) if count==2 else (selected[0]+', from '+selected[1]+', and from '+selected[2])
            if text==phrase:return list(selected)
    raise ValueError('unreviewed protection category or compound list')

def reviewed_payment(record,params):
    from recipes import cost_from_oracle
    cost=record.get('mana_cost','');symbols=re.findall(r'\{([WUBRG])/([WUBRG])\}',cost)
    if not symbols:
        if 'payment_colors' in params:raise ValueError('payment colors require two-color hybrid symbols')
        return cost_from_oracle(record),[]
    choices=params.get('payment_colors')
    if not isinstance(choices,list) or len(choices)!=len(symbols) or any(c not in pair for c,pair in zip(choices,symbols)):
        raise ValueError('one explicit legal color per two-color hybrid symbol required')
    index=iter(choices)
    converted=re.sub(r'\{[WUBRG]/[WUBRG]\}',lambda m:'{'+next(index)+'}',cost)
    return cost_from_oracle(dict(record,mana_cost=converted)),symbols

def mirror_cast_cases(cases,selected):
    """Symmetric stocked boards; switch the active player before legal casts."""
    import copy
    from recipes import step
    result=[]
    for case in cases:
        variant=case['_recipe']['variant']
        if variant+'_1' not in selected:continue
        mirrored=copy.deepcopy(case)
        # Replayed runs may share step objects. Mirror each occurrence once.
        mirrored['steps']=[dict(s) for s in mirrored['steps']]
        mirrored['name']+='_1';mirrored['_recipe']['variant']+='_1'
        for item in mirrored['setup']+mirrored['steps']:
            if item.get('player') in (0,1):item['player']=1-item['player']
        mirrored['steps'].insert(0,step('turn',player=1,value=0));result.append(mirrored)
    return result

# Full Oracle pins for witnesses, separate from engine implementation and flags.
WITNESSES = {
    'white': ('Pillarfield Ox', ['W'], False, 2, 4, ''),
    'blue': ('Vizzerdrix', ['U'], False, 6, 6, ''),
    'black': ('Mass of Ghouls', ['B'], False, 5, 3, ''),
    'red': ('Hill Giant', ['R'], False, 3, 3, ''),
    'green': ('Scaled Wurm', ['G'], False, 7, 6, ''),
    'artifact': ('Phyrexian Hulk', [], True, 5, 4, ''),
    'multicolored': ('Watchwolf', ['G','W'], False, 3, 3, ''),
    'colorless': ("Kozilek's Channeler", [], False, 4, 4, '{T}: Add {C}{C}.'),
}
SPELLS = {
    'growth': ('Giant Growth', ['G'], '{G}', 'Target creature gets +3/+3 until end of turn.'),
    'bounce': ('Unsummon', ['U'], '{U}', "Return target creature to its owner's hand."),
    'shrink': ('Disfigure', ['B'], '{B}', 'Target creature gets -2/-2 until end of turn.'),
    'damage': ('Flame Slash', ['R'], '{R}', 'Flame Slash deals 4 damage to target creature.'),
    'exile': ('Swords to Plowshares', ['W'], '{W}', 'Exile target creature. Its controller gains life equal to its power.'),
    'destroy': ('Terminate', ['B','R'], '{B}{R}', "Destroy target creature. It can't be regenerated."),
    'wrath': ('Wrath of God', ['W'], '{2}{W}{W}', "Destroy all creatures. They can't be regenerated."),
    'sweep_damage': ('Pyroclasm', ['R'], '{1}{R}', 'Pyroclasm deals 2 damage to each creature.'),
}
TARGETS = ('growth','bounce','shrink','damage','exile','destroy')


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout','normal')!='normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line','')
            or declaration.get('oracle_text')!=record.get('oracle_text') or 'variants' not in declaration):
        raise ValueError('defensive component requires a pinned normal single-face creature and reviewed variants')
    canonical={k.lower():k for k in KEYWORDS+SHIELDS};keys=[];protection=[];abilities=[]
    for line in record['oracle_text'].splitlines():
        if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line) and ':' in line:
            abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed defensive reminder')
            text=text.replace(reminder,'')
        text=text.strip()
        if text.startswith('Protection from '):
            protection.extend(protection_values(text[len('Protection from '):]));continue
        tokens=[t.strip().lower() for t in text.split(',')]
        if any(t not in canonical for t in tokens):raise ValueError('unreviewed defensive passive or triggered rules')
        keys.extend(canonical[t] for t in tokens)
    p=declaration.get('params')
    if (not isinstance(p,dict) or set(p)-{'keywords','protection','payment_colors'} or p.get('keywords')!=sorted(keys)
            or p.get('protection',[])!=sorted(protection) or 'protection' in p and not protection
            or len(set(keys))!=len(keys) or len(set(protection))!=len(protection)
            or not (protection or set(keys)&set(SHIELDS))):
        raise ValueError('defensive keywords and protection must explicitly match Oracle')
    if set(keys)&{'Menace','Deathtouch'}:raise ValueError('combined mechanics require dedicated combat scenarios')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')) or not 0<=int(record[field])<=20:
            raise ValueError('defensive component needs bounded numeric base characteristics')
    if int(record['toughness'])==0:raise ValueError('host must survive entry')
    colors=record.get('colors')
    if not isinstance(colors,list) or any(c not in ('W','U','B','R','G') for c in colors) or len(set(colors))!=len(colors):
        raise ValueError('structured unique Oracle colors required')
    mana,hybrids=reviewed_payment(record,p)
    return dict(family='defensive_creature',keywords=sorted(keys),protection=sorted(protection),colors=colors,
                power=int(record['power']),toughness=int(record['toughness']),mana=mana,hybrids=hybrids,remaining_abilities=abilities)


def variants(p):
    result=list(BASE)+['target_'+effect+'_'+str(caster) for effect in TARGETS for caster in (0,1)]
    result+=['wrath_'+str(owner) for owner in (0,1)]+['sweep_damage_'+str(owner) for owner in (0,1)]
    result+=['defend_'+w for w in WITNESSES]+['defense_snapshot']
    if 'Defender' not in p['keywords']:result+=['attack_'+w for w in WITNESSES]+['attack_flying']
    if 'Indestructible' in p['keywords'] and may_target(p,0,0,['B']):result+=['zero_toughness']
    if p.get('hybrids'):result+=['hybrid_payment_other','hybrid_payment_mixed']
    result += [v+'_1' for v in BASE]
    return result


def validate_witnesses(oracle, registry):
    for name,colors,artifact,power,toughness,text in WITNESSES.values():
        r=oracle.get(name,{})
        if (name not in registry or r.get('power')!=str(power) or r.get('toughness')!=str(toughness)
                or r.get('colors')!=colors or ('Artifact' in r.get('type_line',''))!=artifact
                or 'Creature' not in r.get('type_line','') or r.get('oracle_text')!=text):
            raise ValueError('defensive combat witness changed: '+name)
    for name,colors,cost,text in SPELLS.values():
        r=oracle.get(name,{})
        if name not in registry or (r.get('colors'),r.get('mana_cost'),r.get('oracle_text'))!=(colors,cost,text):
            raise ValueError('defensive spell witness changed: '+name)
    for name,text,power,toughness in [('Ornithopter','Flying',0,2),('Grizzly Bears','',2,2)]:
        r=oracle.get(name,{})
        if name not in registry or (r.get('oracle_text'),r.get('power'),r.get('toughness'))!=(text,str(power),str(toughness)):
            raise ValueError('defensive support witness changed: '+name)
    if oracle['Ornithopter'].get('colors')!=[] or 'Artifact' not in oracle['Ornithopter']['type_line']:
        raise ValueError('flying blocker characteristics changed')


def protects(p,colors,creature=False,artifact=False):
    color_names=dict(white='W',blue='U',black='B',red='R',green='G')
    return any(color_names.get(k) in colors or k=='creatures' and creature or k=='artifacts' and artifact
               or k=='monocolored' and len(colors)==1 or k=='multicolored' and len(colors)>1 for k in p['protection'])


def may_target(p,caster,owner,colors):
    keys=set(p['keywords'])
    return not ('Shroud' in keys or 'Hexproof' in keys and caster!=owner or protects(p,colors))


def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,cost_from_oracle
    from creature_recipes import expand as creatures
    base=creatures(name,csvid,declaration,p,[v for v in BASE if v in selected or v+'_1' in selected],registry)
    result=[c for c in base if c['_recipe']['variant'] in selected]+mirror_cast_cases(base,selected)
    if p.get('hybrids'):
        packed=sum(n<<(4*color) for color,n in enumerate(p['mana']))
        for case in result:
            if case['_recipe']['variant'] not in ('cast','snapshot','fresh_attack'):continue
            scripted=[]
            for action in case['steps']:
                if action['op']=='cast':scripted.append(step('choose_payment',player=0,value=packed))
                scripted.append(action)
            case['steps']=scripted
    for variant in ('hybrid_payment_other','hybrid_payment_mixed'):
        if variant not in selected:continue
        original=declaration['params']['payment_colors']
        choices=[(pair[1] if current==pair[0] else pair[0]) if variant=='hybrid_payment_other' or index%2 else current for index,(pair,current) in enumerate(zip(p['hybrids'],original))]
        alt=dict(declaration['params'],payment_colors=choices);mana,_=reviewed_payment(oracle[name],alt)
        cases=creatures(name,csvid,declaration,dict(p,mana=mana),['payment'],registry)
        for case in cases:
            case['name']=f'DefensiveCreatureRecipe_{csvid}_{variant}'
            case['_recipe'].update(variant=variant,payment_colors=choices)
        result+=cases
    for variant in (v for v in selected if v not in BASE):
        if variant.startswith('hybrid_payment_'):continue
        if variant.endswith('_1') and variant[:-2] in BASE:continue
        # Scripted human caster; suffix selects creature ownership, avoiding AI strategic refusal.
        owner=int(variant[-1]) if variant.startswith(('target_','wrath_','sweep_damage_')) else 0
        setup=[card(name,'source',owner)];actions=[];mana=[60,60];life=[20,20];battle=[0,0];battle[owner]=1
        hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];exiles=[list(EXILE),list(EXILE)]
        power=p['power'];toughness=p['toughness'];damage=0;alive=True;keys=set(p['keywords'])
        witness_checks=[]
        if variant.startswith('target_') or variant.startswith(('wrath_','sweep_damage_')) or variant=='zero_toughness':
            effect=variant[len('target_'):].rsplit('_',1)[0] if variant.startswith('target_') else variant.rsplit('_',1)[0]
            caster=0 if variant.startswith('target_') else 1-owner
            if variant=='zero_toughness':effect='shrink';caster=owner
            actions.append(step('turn',player=caster,value=0))
            spell,colors,_,_=SPELLS[effect];copies=(toughness+1)//2 if variant=='zero_toughness' else 1
            targeted=effect in TARGETS;legal=not targeted or may_target(p,caster,owner,colors)
            if effect in ('wrath','sweep_damage'):
                setup+=[card('Grizzly Bears','friendly',owner),card('Grizzly Bears','enemy',1-owner)]
                battle[0]+=1;battle[1]+=1
            for i in range(copies):
                ref='spell'+str(i);setup.append(card(spell,ref,caster,zone='hand'))
                actions.append(step('can_cast',ref=ref,value=int(legal)))
                if not legal:hands[caster].append(spell);continue
                if targeted:actions.append(step('choose_target',player=caster,ref='source'))
                actions += [step('cast',ref=ref,value=1),step('assert_stack',value=1),observe('source','in_play',1),
                            step('resolve'),step('assert_stack',value=0),step('assert_choices_consumed')]
                mana[caster]-=sum(cost_from_oracle(oracle[spell]));graves[caster].append(spell)
                if effect=='growth':power+=3;toughness+=3
                elif effect=='shrink':power-=2;toughness-=2;alive=toughness>0
                elif effect=='bounce':alive=False;hands[owner].append(name)
                elif effect=='exile':alive=False;exiles[owner].append(name);life[owner]+=power
                elif effect in ('destroy','wrath'):alive='Indestructible' in keys
                else:
                    damage=0 if protects(p,colors) else 4 if effect=='damage' else 2
                    alive=damage<toughness or 'Indestructible' in keys
                actions.append(observe('source','in_play',int(alive)))
            if not alive:
                battle[owner]-=1
                if effect not in ('bounce','exile'):graves[owner].append(name)
            if effect in ('wrath','sweep_damage'):
                for player in (0,1):battle[player]-=1;graves[player].append('Grizzly Bears')
                witness_checks += [observe('friendly','in_play',0),observe('enemy','in_play',0)]
        else:
            defending=variant.startswith('defend_') or variant=='defense_snapshot'
            w=variant.split('_',1)[1] if variant!='defense_snapshot' else 'multicolored'
            if w=='flying':wname,colors,artifact,wp,wt='Ornithopter',[],True,0,2
            else:wname,colors,artifact,wp,wt,_=WITNESSES[w]
            setup.append(card(wname,'witness',1-owner));battle[1-owner]+=1
            attacker='witness' if defending else 'source';blocker='source' if defending else 'witness'
            turn=1-owner if defending else owner
            legal=defending or ('Flying' not in keys or w=='flying') and not protects(p,colors,True,artifact)
            actions += [step('turn',player=turn,value=0),step('begin_attackers'),step('attack',ref=attacker,value=1),
                        step('finish_attackers'),step('begin_blockers'),step('block',ref=blocker,other=attacker,value=int(legal)),
                        observe(blocker,'blocking_count',int(legal)),step('advance_combat'),observe(attacker,'blocked',int(legal))]
            if defending:
                first='First strike' in keys or 'Double strike' in keys
                early=first and power>=wt
                outgoing=power*(2 if 'Double strike' in keys and not early else 1)
                damage=0 if early or protects(p,colors,True,artifact) else wp
                alive=damage<toughness or 'Indestructible' in keys
                walive=outgoing<wt
                if not alive:battle[owner]-=1;graves[owner].append(name)
                if not walive:battle[1-owner]-=1;graves[1-owner].append(wname)
                if 'Lifelink' in keys:life[owner]+=outgoing
                actions += [step('combat_damage'),observe('source','in_play',int(alive)),observe('witness','in_play',int(walive))]
                if walive:witness_checks += [observe('witness','power',wp),observe('witness','toughness',wt),observe('witness','damage',outgoing)]
            else:witness_checks += [observe('witness','in_play',1),observe('witness','damage',0)]
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]+witness_checks
        if alive:checks += [observe('source','power',power),observe('source','toughness',toughness),observe('source','damage',damage)]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=mana[player]),
                       step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',LIBRARY),('exile',exiles[player])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                for i,n in enumerate(names) if zone=='library' else enumerate(dict.fromkeys(names)):
                    checks.append(step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) if zone=='library'
                                  else step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']))
        if variant=='defense_snapshot':
            steps=[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+checks+[step('assert_recorded_state')]
        else:steps=actions+checks
        steps.append(step('cleanup'))
        steps.append(observe('source','in_play',int(alive)))
        if alive:steps += [observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',0)]
        result.append(dict(name=f'DefensiveCreatureRecipe_{csvid}_{variant}',card=name,fixture='stocked_zones',seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='defensive_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_abilities=p['remaining_abilities'],untested_keywords=['Flash'] if 'Flash' in keys else [])))
    return result
