"""Explicit positive continuous creature boosts; costs and predicates pinned to Oracle."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

COLORS = {'black':'B', 'blue':'U', 'green':'G', 'red':'R', 'white':'W'}
# These witnesses have no entry or combat triggers. Activated abilities are unused.
WITNESSES = {
    'Hurloon Minotaur': ('2','3','R','Minotaur',''),
    'Balduvian Barbarians': ('3','2','R','Human Barbarian',''),
    'Metallic Sliver': ('1','1','','Sliver',''),
    'Tormented Soul': ('1','1','B','Spirit',"This creature can't block and can't be blocked."),
    'Vampire Interloper': ('2','1','B','Vampire Scout',"Flying\nThis creature can't block."),
    'Scathe Zombies': ('2','2','B','Zombie',''),
    'Elvish Warrior': ('2','3','G','Elf Warrior',''),
    'Wasp Lancer': ('3','2','BU','Faerie Soldier','Flying'),
    'Air Elemental': ('4','4','U','Elemental','Flying'),
    'Crimson Kobolds': ('0','1','R','Kobold',''),
    "Mons's Goblin Raiders": ('1','1','R','Goblin',''),
    'Merfolk of the Pearl Trident': ('1','1','U','Merfolk',''),
    'Hovermyr': ('1','2','','Myr','Flying, vigilance'),
    'Suntail Hawk': ('1','1','W','Bird','Flying'),
    'Basal Thrull': ('1','2','B','Thrull','{T}, Sacrifice this creature: Add {B}{B}.'),
    'Alaborn Grenadier': ('2','2','W','Human Soldier','Vigilance'),
    'Zealous Guardian': ('1','1','UW','Kithkin Soldier','Flash'),
    'Abbey Griffin': ('2','2','W','Griffin','Flying, vigilance'),
    'Grizzly Bears': ('2','2','G','Bear',''),
    'Hill Giant': ('3','3','R','Giant',''),
    'Savannah Lions': ('2','1','W','Cat',''),
}
TRIBES = dict(zip(('Minotaur','Barbarian','Sliver','Spirit','Vampire','Zombie','Warrior','Elf','Faerie',
                  'Elemental','Kobold','Goblin','Merfolk','Myr','Bird','Thrull','Soldier','Kithkin','Griffin'),
                 ('Hurloon Minotaur','Balduvian Barbarians','Metallic Sliver','Tormented Soul','Vampire Interloper',
                  'Scathe Zombies','Elvish Warrior','Elvish Warrior','Wasp Lancer','Air Elemental','Crimson Kobolds',
                  "Mons's Goblin Raiders",'Merfolk of the Pearl Trident','Hovermyr','Suntail Hawk','Basal Thrull',
                  'Alaborn Grenadier','Zealous Guardian','Abbey Griffin')))


def matches(p, record):
    kind = p['predicate']; value = p['value']
    if kind == 'all': return True
    if kind == 'color': return COLORS[value] in record.get('colors', [])
    if kind == 'artifact': return 'Artifact' in record['type_line'].split(' — ')[0].split()
    return value in record['type_line'].split(' — ')[-1].split()


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('lord requires pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p,dict) or set(p) != {'keywords','predicate','value','scope','other','power','toughness'}:
        raise ValueError('explicit lord predicate, controller scope, exclusion, stats and keywords required')
    if (p['scope'] not in ('own','both') or type(p['other']) is not bool
            or any(type(p[k]) is not int or not 0 <= p[k] <= 3 for k in ('power','toughness'))
            or not p['power']+p['toughness']): raise ValueError('bounded positive continuous boost required')
    kind = p['predicate']; value = p['value']
    if not ((kind == 'all' and value == '') or (kind == 'color' and value in COLORS)
            or (kind == 'subtype' and value in TRIBES) or (kind == 'artifact' and value == '')):
        raise ValueError('unreviewed lord predicate or witness')
    noun = '' if kind == 'all' else 'artifact ' if kind == 'artifact' else value+' '
    text = ('Other ' if p['other'] else '')+noun+'creatures'+(' you control' if p['scope']=='own' else '')
    text += f" get +{p['power']}/+{p['toughness']}."
    expected = {text[0].upper()+text[1:]}
    if not p['other']: expected.add('All '+noun+'creatures'+(' you control' if p['scope']=='own' else '')+f" get +{p['power']}/+{p['toughness']}.")
    rules=[];keys=[];abilities=[];canonical={k.lower():k for k in KEYWORDS}
    for line in record['oracle_text'].splitlines():
        if line in expected:rules.append(line);continue
        if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):
            abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS | EXTRA_REMINDERS:raise ValueError('unreviewed lord reminder')
            text=text.replace(reminder,'')
        tokens=[k.strip().lower() for k in text.strip().split(',')]
        if any(k not in canonical for k in tokens):raise ValueError('unreviewed passive or triggered lord clause')
        keys += [canonical[k] for k in tokens]
    if (len(rules)!=1 or p['keywords']!=sorted(keys) or len(set(keys))!=len(keys)
            or set(keys)&{'Menace','Deathtouch','Double strike','Lifelink'}
            or not set(keys)<=set(record.get('keywords',[]))):raise ValueError('lord parameters disagree with pinned whole passive text')
    for k in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(k,'')) or not 0 <= int(record[k]) <= 20:raise ValueError('numeric lord stats required')
    if int(record['toughness'])==0:raise ValueError('lord must survive entry')
    return dict(p,family='lord_creature',base_power=int(record['power']),base_toughness=int(record['toughness']),
                mana=cost_from_oracle(record),remaining_abilities=abilities,legendary='Legendary' in record['type_line'])


def variants(p):
    modes=['static','tapped_source','sick_source','cleanup','cast','pending_snapshot','snapshot','source_bounce',
           'bounce_recast','destroy','blink_source','blink_witness','opposing_source','pump','damage','combat_snapshot','empty_zones']
    if not p['legendary']: modes += ['two_sources','remove_one_source']
    result=[v+'_'+str(owner) for owner in (0,1) for v in modes]
    return result+['payment','insufficient_mana']+(['wrong_color'] if any(p['mana'][1:]) else [])


def validate_witnesses(oracle, registry):
    for name,(power,tough,colors,types,text) in WITNESSES.items():
        r=oracle.get(name,{})
        if (name not in registry or 'Creature' not in r.get('type_line','') or r.get('card_faces') or r.get('selected_face')
                or (r.get('power'),r.get('toughness'),''.join(sorted(r.get('colors',[]))),r.get('type_line','').split(' — ')[-1],r.get('oracle_text'))
                != (power,tough,colors,types,text)):
            raise ValueError('lord witness changed: '+name)
    for name,cost,text in [('Unsummon','{U}',"Return target creature to its owner's hand."),
        ('Cloudshift','{W}','Exile target creature you control, then return that card to the battlefield under your control.'),
        ('Giant Growth','{G}','Target creature gets +3/+3 until end of turn.')]:
        if name not in registry or (oracle.get(name,{}).get('mana_cost'),oracle.get(name,{}).get('oracle_text')) != (cost,text):
            raise ValueError('lord spell witness changed: '+name)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card,step,observe,cost_from_oracle
    positive = TRIBES[p['value']] if p['predicate']=='subtype' else 'Hovermyr' if p['predicate']=='artifact' else {
        'black':'Scathe Zombies','blue':'Air Elemental','green':'Grizzly Bears','red':'Hill Giant','white':'Savannah Lions','':'Grizzly Bears'}[p['value']]
    negative = next(n for n in ('Hill Giant','Grizzly Bears','Savannah Lions','Hovermyr') if not matches(p,oracle[n])) if p['predicate']!='all' else 'Hill Giant'
    result=[]
    for variant in selected:
        owner=int(variant.endswith('_1'));v=variant[:-2] if variant.endswith(('_0','_1')) else variant
        fixture='empty_main' if v=='empty_zones' else 'stocked_zones'
        hands=[list(HAND) if fixture=='stocked_zones' else [] for _ in (0,1)]
        graves=[list(GRAVE) if fixture=='stocked_zones' else [] for _ in (0,1)]
        mana=[[10]*6 for _ in (0,1)];life=[20,20];setup=[];pre=[];run=[];sources=[];witnesses={};battle=[0,0]
        casting=v in ('cast','pending_snapshot','payment','insufficient_mana','wrong_color')
        setup.append(card(name,'source',owner,'hand' if casting else 'battlefield'))
        if casting:hands[owner].append(name)
        else:sources.append(('source',owner));battle[owner]+=1
        if v=='tapped_source':setup[0]['state']=['tapped']
        if v=='sick_source':setup[0]['state']=['sick']
        for player in (owner,1-owner):
            for label,n in (('match',positive),('other',negative)):
                ref=('own_' if player==owner else 'opponent_')+label
                setup.append(card(n,ref,player));witnesses[ref]=(n,player);battle[player]+=1
                # A missing-color negative requires no alternate legal mana
                # source. Basal Thrull otherwise supplies the black cost.
                if v=='wrong_color' and n=='Basal Thrull':setup[-1]['state']=['tapped']
        if owner:pre.append(step('turn',player=owner,value=0))
        if v in ('two_sources','remove_one_source','opposing_source'):
            player=1-owner if v=='opposing_source' else owner
            setup.append(card(name,'second',player));sources.append(('second',player));battle[player]+=1
        def stats(extra=None):
            extra=extra or {};out=[]
            for ref,n,player in [(ref,name,player) for ref,player in sources]+[(ref,n,player) for ref,(n,player) in witnesses.items()]:
                base=oracle[n];bonus=sum(matches(p,base) and (p['scope']=='both' or player==controller) and (not p['other'] or ref!=source) for source,controller in sources)
                out += [observe(ref,'in_play',1),observe(ref,'power',int(base['power'])+bonus*p['power']+extra.get(ref,0)),
                        observe(ref,'toughness',int(base['toughness'])+bonus*p['toughness']+extra.get(ref,0))]
            return out
        def spell(n,ref,target,caster=0):
            setup.append(card(n,ref,caster,'hand'));mana[caster]=[a-b for a,b in zip(mana[caster],cost_from_oracle(oracle[n]))];graves[caster].append(n)
            return [step('choose_target',player=caster,ref=target),step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
        if v in ('payment','insufficient_mana','wrong_color'):
            mana[0]=list(p['mana']) if v=='payment' else [0]*6 if v=='insufficient_mana' else list(p['mana'])
            if v=='wrong_color':
                color=next(i for i in range(1,6) if mana[0][i]);mana[0][color]=0;mana[0][1 if color!=1 else 2]+=p['mana'][color]
            pre += [step('set_mana',player=0,color=i,value=n) for i,n in enumerate(mana[0])]
        if casting:
            run += stats()
            legal=v not in ('insufficient_mana','wrong_color')
            run += [step('cast',ref='source',value=int(legal))]
            if legal:
                mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])];hands[owner].remove(name)
                run += [step('assert_stack',value=1)]+stats()
                cut=len(run)
                run += [step('resolve'),step('assert_choices_consumed')];sources.append(('source',owner));battle[owner]+=1;run += stats()
            else:run += [observe('source','in_hand',1),step('assert_stack',value=0)]+stats()
        else:
            run += stats()
            cut=0
            if v in ('source_bounce','bounce_recast','destroy','blink_source','remove_one_source'):
                if v=='destroy':
                    run += [step('destroy',ref='source')];sources.remove(('source',owner));battle[owner]-=1;graves[owner].append(name)
                elif v in ('source_bounce','bounce_recast','remove_one_source'):
                    run += spell('Unsummon','bounce','source');sources.remove(('source',owner));battle[owner]-=1;hands[owner].append(name)
                    run += [step('bind_hand',player=owner,value=csvid,ref='source'),observe('source','in_hand',1)]+stats()
                    if v=='bounce_recast':
                        run += [step('cast',ref='source',value=1),step('resolve'),step('assert_choices_consumed')]
                        sources.append(('source',owner));battle[owner]+=1;hands[owner].remove(name);mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])]
                else:
                    run += spell('Cloudshift','blink','source',owner)
                    run += [step('bind_permanent',player=owner,value=csvid,ref='source')]
                run += stats()
            elif v=='blink_witness':
                run += spell('Cloudshift','blink','own_match',owner)
                run += [step('bind_permanent',player=owner,value=registry[positive]['csvid'],ref='own_match')]+stats()
            elif v=='pump':
                run += spell('Giant Growth','pump','own_match')+stats({'own_match':3})
                run += [step('cleanup')]+stats()
            elif v=='cleanup':run += [step('cleanup')]+stats()
            elif v in ('damage','combat_snapshot'):
                run += [step('begin_attackers'),step('attack',ref='own_match',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]+stats()
                cut=len(run)
                n=oracle[positive];bonus=int(matches(p,n));power=int(n['power'])+p['power']*bonus
                life[1-owner]-=power
                run += [step('combat_damage'),step('assert_life',player=1-owner,value=life[1-owner]),step('end_combat'),step('cleanup')]+stats()
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(mana[player])),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',EXILE if fixture=='stocked_zones' else []),('library',LIBRARY if fixture=='stocked_zones' else [])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']) for i,n in enumerate(names)]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[n]['csvid'],value=count) for n,count in Counter(hands[player]+graves[player]).items()]
        actions=pre+run+checks
        if v in ('pending_snapshot','combat_snapshot','snapshot'):
            actions=pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'LordCreatureRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=actions,
                           _recipe=dict(family='lord_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_abilities=p['remaining_abilities'],untested_keywords=list(p['keywords']))))
    return result
