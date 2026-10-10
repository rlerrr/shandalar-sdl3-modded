"""Reviewed evasion components, exercised through normal combat declarations."""
import re
from creature_recipes import KEYWORDS, REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

EVASION = ('Fear', 'Intimidate', 'Shadow', 'Horsemanship', 'Forestwalk',
           'Islandwalk', 'Swampwalk', 'Mountainwalk', 'Plainswalk')
LANDS = dict(Forestwalk='Forest', Islandwalk='Island', Swampwalk='Swamp',
             Mountainwalk='Mountain', Plainswalk='Plains')
EXTRA_REMINDERS = {
    '(This creature can block or be blocked by only creatures with shadow.)',
    "(This creature can't be blocked except by creatures with horsemanship.)",
    "(This creature can't be blocked except by artifact creatures and/or black creatures.)",
    "(This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)",
    '(This creature deals combat damage before creatures without first strike, it can\'t be blocked as long as defending player controls a Forest, and attacking doesn\'t cause this creature to tap.)',
} | {f"(This creature can't be blocked as long as defending player controls {'an' if land=='Island' else 'a'} {land}.)" for land in LANDS.values()}

# Explicit witness characteristics, independently pinned to Oracle below.
# None of the witnesses' activated abilities are used.
WITNESSES = {
    'white': ('Savannah Lions', [], ['W'], False, 2, 1, ''),
    'blue': ('Merfolk of the Pearl Trident', [], ['U'], False, 1, 1, ''),
    'black': ('Scathe Zombies', [], ['B'], False, 2, 2, ''),
    'red': ("Mons's Goblin Raiders", [], ['R'], False, 1, 1, ''),
    'green': ('Grizzly Bears', [], ['G'], False, 2, 2, ''),
    'artifact': ('Ornithopter', ['Flying'], [], True, 0, 2, 'Flying'),
    'colored_artifact': ('Tidehollow Strix', ['Flying', 'Deathtouch'], ['B','U'], True, 2, 1,
                         'Flying\nDeathtouch (Any amount of damage this deals to a creature is enough to destroy it.)'),
    'colorless': ('Scion of Ugin', ['Flying'], [], False, 4, 4, 'Flying'),
    'flying': ('Suntail Hawk', ['Flying'], ['W'], False, 1, 1, 'Flying'),
    'reach': ('Giant Spider', ['Reach'], ['G'], False, 2, 4, 'Reach (This creature can block creatures with flying.)'),
    'shadow': ('Dauthi Marauder', ['Shadow'], ['B'], False, 3, 1,
               'Shadow (This creature can block or be blocked by only creatures with shadow.)'),
    'shadow_flying': ('Stronghold Overseer', ['Flying','Shadow'], ['B'], False, 5, 5,
                      'Flying\nShadow (This creature can block or be blocked by only creatures with shadow.)\n{B}{B}: Creatures with shadow get +1/+0 until end of turn and creatures without shadow get -1/-0 until end of turn.'),
    'horse': ('Shu Cavalry', ['Horsemanship'], ['W'], False, 2, 2,
              "Horsemanship (This creature can't be blocked except by creatures with horsemanship.)"),
}
BASE = ('cast','payment','insufficient_mana','snapshot','fresh_attack','unblocked','tapped_attacker','tapped_blocker')


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout','normal')!='normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in record.get('type_line','')
            or declaration.get('oracle_text')!=record.get('oracle_text') or 'variants' not in declaration):
        raise ValueError('evasion requires a pinned normal single-face creature and reviewed variants')
    canonical={k.lower():k for k in KEYWORDS+EVASION};keys=[];abilities=[];unblockable=False
    for line in record['oracle_text'].splitlines():
        if re.match(r'^(?:\{[^}]+\}|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line) and ':' in line:
            abilities.append(line);continue
        if line in ("This creature can't be blocked.", record['name']+" can't be blocked."):
            if unblockable:raise ValueError('duplicate unblockable clause')
            unblockable=True;continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed evasion reminder')
            text=text.replace(reminder,'')
        tokens=[t.strip().lower() for t in text.strip().split(',')]
        if any(t not in canonical for t in tokens):raise ValueError('evasion has unreviewed passive or triggered rules')
        keys.extend(canonical[t] for t in tokens)
    p=declaration.get('params')
    if (not isinstance(p,dict) or set(p)-{'keywords','unblockable'} or p.get('keywords')!=sorted(keys)
            or len(set(keys))!=len(keys) or 'unblockable' in p and p['unblockable'] is not True
            or bool(p.get('unblockable'))!=unblockable or not (unblockable or set(keys)&set(EVASION))):
        raise ValueError('evasion requires explicit matching keywords and unblockability')
    if set(keys)&{'Menace','Defender','Trample','Deathtouch'}:
        raise ValueError('combined evasion requires dedicated menace/defender/trample/deathtouch scenarios')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')) or not 0<=int(record[field])<=20:
            raise ValueError('evasion needs bounded numeric base characteristics')
    if int(record['toughness'])==0:raise ValueError('evasion host must survive entry')
    colors=record.get('colors')
    if not isinstance(colors,list) or any(c not in ('W','U','B','R','G') for c in colors) or len(set(colors))!=len(colors):
        raise ValueError('evasion requires structured unique Oracle colors')
    return dict(family='evasion_creature',keywords=sorted(keys),unblockable=unblockable,colors=colors,
                artifact='Artifact' in record['type_line'],power=int(record['power']),toughness=int(record['toughness']),
                mana=cost_from_oracle(record),remaining_abilities=abilities)


def variants(p):
    result=list(BASE)+['attack_'+k for k in WITNESSES]+['defend_'+k for k in ('green','flying','shadow','shadow_flying','horse')]
    result+=['opponent_attack','opponent_defend','cleanup_evasion','evasion_damage','combat_snapshot']
    if set(p['keywords'])&set(LANDS):
        result+=['land_matching','land_wrong','land_tapped','land_attacker_only','land_removed','opponent_land_matching']
    if 'First strike' in p['keywords'] and 'Shadow' not in p['keywords']:result.append('first_strike')
    return result


def validate_witnesses(oracle, registry):
    for name,keys,colors,artifact,power,toughness,text in WITNESSES.values():
        r=oracle.get(name,{})
        if (name not in registry or r.get('power')!=str(power) or r.get('toughness')!=str(toughness)
                or r.get('colors')!=colors or ('Artifact' in r.get('type_line',''))!=artifact
                or 'Creature' not in r.get('type_line','') or r.get('oracle_text')!=text):
            raise ValueError('evasion witness changed: '+name)
    for name in LANDS.values():
        r=oracle.get(name,{})
        if name not in registry or r.get('type_line')!='Basic Land — '+name:
            raise ValueError('evasion basic-land witness changed: '+name)
    r=oracle.get('Boomerang',{})
    if 'Boomerang' not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=('{U}{U}',"Return target permanent to its owner's hand."):
        raise ValueError('evasion response witness changed: Boomerang')


def characteristics(witness):
    name,keys,colors,artifact,power,toughness,_=WITNESSES[witness]
    return dict(name=name,keywords=keys,colors=colors,artifact=artifact,unblockable=False,power=power,toughness=toughness)


def may_block(attacker, blocker, defending_lands=()):
    """Oracle keyword rules; never inspect engine flags or handler results."""
    a=set(attacker['keywords']);b=set(blocker['keywords'])
    if attacker.get('unblockable') or any(LANDS[k] in defending_lands for k in a&set(LANDS)):return False
    if ('Shadow' in a)!=('Shadow' in b):return False
    if 'Horsemanship' in a and 'Horsemanship' not in b:return False
    if 'Flying' in a and not b&{'Flying','Reach'}:return False
    if 'Fear' in a and not blocker['artifact'] and 'B' not in blocker['colors']:return False
    if 'Intimidate' in a and not blocker['artifact'] and not set(attacker['colors'])&set(blocker['colors']):return False
    return True


def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,cast
    from creature_recipes import expand as creatures
    base=set(BASE)|{'first_strike'}
    result=creatures(name,csvid,declaration,p,[v for v in selected if v in base],registry)
    for variant in (v for v in selected if v not in base):
        defending=variant.startswith('defend_') or variant=='opponent_defend'
        owner=1 if variant in ('opponent_attack','opponent_defend','opponent_land_matching') else 0
        witness=variant.split('_',1)[1] if variant.startswith(('attack_','defend_')) else 'green'
        if variant=='evasion_damage':witness='colorless'
        setup=[card(name,'source',owner),card(WITNESSES[witness][0],'witness',1-owner)]
        hands=[list(HAND),list(HAND)];graves=[list(GRAVE),list(GRAVE)];mana=[60,60];life=[20,20]
        battle=[1,1];lands=[];pre=[]
        matching=next((LANDS[k] for k in p['keywords'] if k in LANDS),None)
        if variant.startswith('land_') or variant=='opponent_land_matching' or variant=='evasion_damage' and matching:
            land=next(n for n in LANDS.values() if n!=matching) if variant=='land_wrong' else matching
            land_owner=owner if variant=='land_attacker_only' else 1-owner
            setup.append(card(land,'land',land_owner));battle[land_owner]+=1
            if variant=='land_tapped':setup[-1]['state']=['tapped']
            if land_owner!=owner:lands=[land]
        if variant=='cleanup_evasion':pre+=[step('cleanup'),step('turn',player=owner,value=0)]
        if variant=='land_removed':
            setup.append(card('Boomerang','response',owner,zone='hand'))
        if defending:
            attacker=characteristics(witness);blocker=p;attacker_ref='witness';blocker_ref='source';turn=1-owner
        else:attacker=p;blocker=characteristics(witness);attacker_ref='source';blocker_ref='witness';turn=owner
        actions=[step('turn',player=turn,value=0),step('begin_attackers'),step('attack',ref=attacker_ref,value=1),
                 observe(attacker_ref,'attacking',1),observe(attacker_ref,'tapped',int('Vigilance' not in attacker['keywords'])),step('finish_attackers')]
        if variant=='land_removed':
            # An instant legally removes the defending land before blockers.
            actions+=cast(ref='response',target='land')
            mana[owner]-=2;hands[1-owner].append(matching);graves[owner].append('Boomerang');battle[1-owner]-=1;lands=[]
        legal=int(may_block(attacker,blocker,lands))
        actions+=[step('begin_blockers'),step('block',ref=blocker_ref,other=attacker_ref,value=legal),
                  observe(blocker_ref,'blocking_count',legal),step('advance_combat'),observe(attacker_ref,'blocked',legal)]
        if variant=='evasion_damage':
            assert not legal,'evasion damage requires a reviewed denied blocker'
            total=p['power']*(2 if 'Double strike' in p['keywords'] else 1)
            life[1-owner]-=total
            if 'Lifelink' in p['keywords']:life[owner]+=total
            actions+=[step('combat_damage'),observe('source','in_play',1),observe('witness','in_play',1)]
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for ref,stats in (('source',p),('witness',characteristics(witness))):
            checks+=[observe(ref,'power',stats['power']),observe(ref,'toughness',stats['toughness']),observe(ref,'damage',0)]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=mana[player]),
                       step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',LIBRARY),('exile',EXILE)):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                if zone=='library':checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) for i,n in enumerate(names)]
                else:checks += [step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']) for n in dict.fromkeys(names)]
        if variant=='combat_snapshot':
            steps=pre+[step('capture')]+actions+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+actions+checks+[step('assert_recorded_state')]
        else:steps=pre+actions+checks
        steps+=[step('cleanup'),observe('source','in_play',1),observe('source','power',p['power']),observe('source','toughness',p['toughness'])]
        result.append(dict(name=f'EvasionCreatureRecipe_{csvid}_{variant}',card=name,fixture='stocked_zones',seed=123,setup=setup,steps=steps,
                           _recipe=dict(family='evasion_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_abilities=p['remaining_abilities'],untested_keywords=['Flash'] if 'Flash' in p['keywords'] else [])))
    return result
