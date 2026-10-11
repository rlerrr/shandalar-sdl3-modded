"""Pinned fixed entry counters and bloodthirst through real casts and damage."""
import re
from collections import Counter
from creature_recipes import KEYWORDS, REMINDERS
from defensive_creature_recipes import EXTRA_REMINDERS
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

WORDS = {1:'a',2:'two',3:'three',4:'four',5:'five',6:'six',7:'seven'}
SPELLS = {
    'Unsummon': ('{U}',"Return target creature to its owner's hand."),
    'Cloudshift': ('{W}','Exile target creature you control, then return that card to the battlefield under your control.'),
    'Giant Growth': ('{G}','Target creature gets +3/+3 until end of turn.'),
    'Lightning Bolt': ('{R}','Lightning Bolt deals 3 damage to any target.'),
    'Death of a Thousand Stings': ('{4}{B}','Target player loses 1 life and you gain 1 life.\nAt the beginning of your upkeep, if you have more cards in hand than each opponent, you may return this card from your graveyard to your hand.'),
    'Essence Scatter': ('{1}{U}','Counter target creature spell.'),
    'Swords to Plowshares': ('{W}','Exile target creature. Its controller gains life equal to its power.'),
}
WITNESSES = {'Scaled Wurm':('7','6',''), 'Savannah Lions':('2','1',''), 'Glorious Anthem':(None,None,'Creatures you control get +1/+1.')}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','') or declaration.get('oracle_text')!=record.get('oracle_text')
            or 'variants' not in declaration):raise ValueError('entry counters require pinned normal creature and reviewed variants')
    p=declaration.get('params')
    if not isinstance(p,dict) or set(p)!={'keywords','mechanic','amount'}:raise ValueError('explicit entry mechanic, amount and keywords required')
    mode=p['mechanic'];n=p['amount']
    if mode not in ('fixed','bloodthirst') or type(n) is not int or n not in WORDS:raise ValueError('unsupported entry counter mechanic or bounded amount')
    counter=f"{WORDS[n]} +1/+1 counter{'s' if n!=1 else ''} on it."
    if mode=='fixed':
        refs=('This creature',record['name'],record['name'].split(',')[0])
        expected={f'{ref} enters{suffix} with {counter}' for ref in refs for suffix in ('',' the battlefield')}
    else:
        clause=f'Bloodthirst {n}'
        expected={clause,clause+f' (If an opponent was dealt damage this turn, this creature enters with {counter})'}
    rules=[];keys=[];abilities=[];canonical={k.lower():k for k in KEYWORDS}
    for line in record['oracle_text'].splitlines():
        if line in expected:rules.append(line);continue
        if ':' in line and re.match(r'^(?:\{|Sacrifice |Discard |Remove |Exile |Pay |Tap |Untap |Return )',line):
            abilities.append(line);continue
        text=line
        for reminder in re.findall(r'\([^\n]*?\)',line):
            if reminder not in REMINDERS|EXTRA_REMINDERS:raise ValueError('unreviewed entry counter reminder')
            text=text.replace(reminder,'')
        tokens=[k.strip().lower() for k in text.strip().split(',')]
        if any(k not in canonical for k in tokens):raise ValueError('unreviewed entry passive or triggered rule')
        keys += [canonical[k] for k in tokens]
    if (len(rules)!=1 or p['keywords']!=sorted(keys) or len(keys)!=len(set(keys))
            or not set(keys)<=set(record.get('keywords',[]))
            or mode=='bloodthirst' and 'Bloodthirst' not in record.get('keywords',[])):
        raise ValueError('entry parameters must agree with passive text and structured keywords')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+',record.get(field,'')) or not 0 <= int(record[field]) <= 20:raise ValueError('bounded numeric entry stats required')
    if int(record['toughness'])+(n if mode=='fixed' else 0)<=0:raise ValueError('entry creature must survive without conditional counters')
    return dict(p,family='entry_counter_creature',power=int(record['power']),toughness=int(record['toughness']),
                mana=cost_from_oracle(record),remaining_abilities=abilities,legendary='Legendary' in record['type_line'])


def variants(p):
    modes=['cast','pending_snapshot','snapshot','cleanup','blink','blink_snapshot','bounce','bounce_recast','growth_blink',
           'pump','destroy','exile','tapped_entry','sick_entry','empty_zones','anthem','fresh_attack']
    if not p['legendary']:modes+=['two_copies']
    if p['mechanic']=='bloodthirst':
        modes+=['opponent_damage','self_damage','creature_damage','combat_damage','multiple_damage','life_loss',
                'previous_damage','late_damage','damage_snapshot','blink_after_damage','blink_new_turn','recast_after_damage','recast_new_turn']
        if not p['legendary']:modes+=['two_damaged_copies']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]+['payment','insufficient_mana','countered']+(['wrong_color'] if any(p['mana'][1:]) else [])


def validate_witnesses(oracle, registry):
    for name,fields in SPELLS.items():
        r=oracle.get(name,{})
        if name not in registry or (r.get('mana_cost'),r.get('oracle_text'))!=fields:raise ValueError('entry spell witness changed: '+name)
    for name,fields in WITNESSES.items():
        r=oracle.get(name,{})
        if name not in registry or r.get('layout')!='normal' or r.get('card_faces') or (r.get('power'),r.get('toughness'),r.get('oracle_text'))!=fields:
            raise ValueError('entry permanent witness changed: '+name)


def expand(name,csvid,declaration,p,selected,registry,oracle):
    from recipes import card,step,observe,cost_from_oracle
    result=[];mode=p['mechanic'];n=p['amount']
    for variant in selected:
        owner=int(variant.endswith('_1'));v=variant[:-2] if variant.endswith(('_0','_1')) else variant
        fixture='empty_main' if v=='empty_zones' else 'stocked_zones';full=fixture=='stocked_zones'
        hands=[list(HAND) if full else [] for _ in (0,1)];graves=[list(GRAVE) if full else [] for _ in (0,1)];exile=[list(EXILE) if full else [] for _ in (0,1)]
        mana=[[10]*6 for _ in (0,1)];life=[20,20];battle=[0,0];setup=[];pre=[];run=[]
        # Repeated expensive casts must not invoke a human mana-color picker.
        # Dedicated payment cases still start with exactly the printed cost.
        if v in ('bounce_recast','two_copies','recast_after_damage','recast_new_turn','two_damaged_copies') and p['mana'][0]*2>10:
            mana[owner][0]=20;pre.append(step('set_mana',player=owner,color=0,value=20))
        existing=v in ('tapped_entry','sick_entry');setup.append(card(name,'source',owner,'battlefield' if existing else 'hand'))
        if existing:setup[0]['state']=['tapped' if v=='tapped_entry' else 'sick'];battle[owner]+=1
        else:hands[owner].append(name)
        if owner:pre.append(step('turn',player=owner,value=0))
        expected=n if mode=='fixed' else 0;boost=0
        def stats(ref='source',amount=None,bonus=0):
            amount=expected if amount is None else amount
            return [step('require_card',ref=ref,property='in_play',value=1),observe(ref,'plus1_counters',amount),observe(ref,'power',p['power']+amount+bonus+boost),observe(ref,'toughness',p['toughness']+amount+bonus+boost)]
        def pay(cost,player):mana[player]=[a-b for a,b in zip(mana[player],cost)]
        def cast(ref='source'):
            pay(p['mana'],owner);hands[owner].remove(name);battle[owner]+=1
            return [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]+stats(ref)
        def spell(nm,ref,target=None,caster=0,target_player=None):
            setup.append(card(nm,ref,caster,'hand'));pay(cost_from_oracle(oracle[nm]),caster);graves[caster].append(nm)
            choice=step('choose_player',player=caster,value=target_player) if target_player is not None else step('choose_target',player=caster,ref=target)
            return [choice,step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
        def bolt(ref,target_player=None,target=None):
            actions=spell('Lightning Bolt',ref,target=target,target_player=target_player)
            if target_player is not None:life[target_player]-=3
            return actions
        def bounce(recast=False):
            nonlocal expected
            actions=spell('Unsummon','bounce','source');hands[owner].append(name);battle[owner]-=1
            actions += [step('bind_hand',player=owner,value=csvid,ref='source'),observe('source','in_hand',1),observe('source','plus1_counters',0)]
            if recast:actions+=cast()
            return actions
        def blink():
            return spell('Cloudshift','blink','source',owner)+[step('bind_permanent',player=owner,value=csvid,ref='source')]+stats()
        if v in ('payment','insufficient_mana','wrong_color'):
            mana[0]=list(p['mana']) if v!='insufficient_mana' else [0]*6
            if v=='wrong_color':
                color=next(i for i in range(1,6) if mana[0][i]);mana[0][color]=0;mana[0][1 if color!=1 else 2]+=p['mana'][color]
            pre += [step('set_mana',player=0,color=i,value=value) for i,value in enumerate(mana[0])]
        damaged=v in ('opponent_damage','multiple_damage','damage_snapshot','blink_after_damage','blink_new_turn','recast_after_damage','recast_new_turn','two_damaged_copies')
        if damaged:
            run+=bolt('damage',target_player=1-owner);expected=n
            if v=='multiple_damage':run+=bolt('second_damage',target_player=1-owner)
        if v=='self_damage':run+=bolt('self_damage',target_player=owner)
        if v=='creature_damage':
            setup.append(card('Scaled Wurm','victim',1-owner));battle[1-owner]+=1
            run+=bolt('creature_damage',target='victim')+[observe('victim','in_play',1),observe('victim','damage',3),observe('victim','plus1_counters',0)]
        if v=='life_loss':
            run+=spell('Death of a Thousand Stings','life_loss',target_player=1-owner)
            life[1-owner]-=1;life[0]+=1
        if v=='previous_damage':
            run+=bolt('previous_damage',target_player=1-owner)+[step('advance_turn'),step('advance_turn')]
        if v=='combat_damage':
            setup.append(card('Savannah Lions','attacker',owner));battle[owner]+=1;life[1-owner]-=2;expected=n
            run += [step('begin_attackers'),step('attack',ref='attacker',value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat'),step('combat_damage'),step('end_combat')]
        if v=='anthem':setup.append(card('Glorious Anthem','anthem',owner));battle[owner]+=1;boost=1
        cut=len(run)
        if existing:
            run+=stats()+[observe('source','tapped',int(v=='tapped_entry')),observe('source','sick',int(v=='sick_entry'))]
        elif v in ('insufficient_mana','wrong_color'):
            run += [step('cast',ref='source',value=0),observe('source','in_hand',1),observe('source','plus1_counters',0),step('assert_stack',value=0)]
        elif v=='countered':
            setup.append(card('Essence Scatter','counter',owner,'hand'));pay(p['mana'],owner);pay(cost_from_oracle(oracle['Essence Scatter']),owner)
            hands[owner].remove(name);graves[owner]+=[name,'Essence Scatter']
            run += [step('choose_priority',ref='counter'),step('cast',ref='source',value=0),step('resolve'),step('assert_choices_consumed')]
        elif v=='late_damage':
            pay(p['mana'],owner);hands[owner].remove(name);battle[owner]+=1;expected=n
            run += [step('cast',ref='source',value=1),step('assert_stack',value=1)]
            # The scripted response is cast with the source already pending.
            setup.append(card('Lightning Bolt','late_damage',0,'hand'));pay(cost_from_oracle(oracle['Lightning Bolt']),0);graves[0].append('Lightning Bolt');life[1-owner]-=3
            run += [step('choose_player',player=0,value=1-owner),step('cast',ref='late_damage',value=1),step('assert_stack',value=2),step('resolve'),step('assert_choices_consumed')]+stats()
        else:
            if v=='pending_snapshot':
                pay(p['mana'],owner);hands[owner].remove(name);battle[owner]+=1
                run += [step('cast',ref='source',value=1),step('assert_stack',value=1),observe('source','plus1_counters',0)];cut=len(run)
                run += [step('resolve'),step('assert_choices_consumed')]+stats()
            else:run+=cast()
            if v in ('two_copies','two_damaged_copies'):
                setup.append(card(name,'second',owner,'hand'));hands[owner].append(name);run+=cast('second')+stats()
            if v in ('blink','blink_snapshot','blink_after_damage','growth_blink','blink_new_turn'):
                if v=='growth_blink':run+=spell('Giant Growth','growth','source')+stats(bonus=3)
                if v=='blink_new_turn':
                    run += [step('advance_turn'),step('advance_turn')]+stats();expected=0
                run+=blink()
                if v=='blink_snapshot':cut=max(i for i,s in enumerate(run) if s['op']=='resolve')
            if v in ('bounce','bounce_recast','recast_after_damage','recast_new_turn'):
                if v=='recast_new_turn':run += [step('advance_turn'),step('advance_turn')]+stats();expected=0
                run+=bounce(v!='bounce')
            if v=='pump':run+=spell('Giant Growth','growth','source')+stats(bonus=3)+[step('cleanup')]+stats()
            if v=='cleanup':run += [step('cleanup')]+stats()+[step('cleanup')]+stats()
            if v=='destroy':
                run += [step('destroy',ref='source'),observe('source','in_play',0),observe('source','plus1_counters',0)];battle[owner]-=1;graves[owner].append(name)
            if v=='exile':
                run += spell('Swords to Plowshares','exile','source')+[observe('source','in_play',0),observe('source','plus1_counters',0)]
                battle[owner]-=1;exile[owner].append(name);life[owner]+=p['power']+expected+boost
            if v=='fresh_attack':
                legal=int('Haste' in p['keywords'] and 'Defender' not in p['keywords'])
                run += [step('begin_attackers'),step('attack',ref='source',value=legal),observe('source','attacking',legal),step('finish_attackers')]+stats()
        checks=[step('assert_stack',value=0),step('assert_choices_consumed')]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(mana[player])),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('exile',exile[player]),('library',LIBRARY if full else [])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[nm]['csvid']) if zone=='library' else step('assert_zone_contains',player=player,zone=zone,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
            checks += [step('assert_hand_graveyard_total',player=player,index=registry[nm]['csvid'],value=count) for nm,count in Counter(hands[player]+graves[player]).items()]
        actions=pre+run+checks
        if v in ('snapshot','pending_snapshot','blink_snapshot','damage_snapshot'):
            actions=pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'EntryCounterCreatureRecipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=actions,
                           _recipe=dict(family='entry_counter_creature',version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_abilities=p['remaining_abilities'],untested_keywords=list(p['keywords']))))
    return result
