"""Oracle-pinned tap damage abilities: costs, targets, and real resolution."""
import re
from creature_recipes import KEYWORDS, REMINDERS, variants as creature_variants
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY

WITNESSES = {
    'Scaled Wurm': ('7', '6', ''),
    'Air Elemental': ('4', '4', 'Flying'),
    'Muck Rats': ('1', '1', ''),
    'Suntail Hawk': ('1', '1', 'Flying'),
    'Darksteel Myr': ('0', '1', 'Indestructible (Damage and effects that say "destroy" don\'t destroy this creature. If its toughness is 0 or less, it still dies.)'),
}
SPELLS = {
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Vitalize': ('{G}', 'Untap all creatures you control.'),
    'Robe of Mirrors': ('{U}', "Enchant creature (Target a creature as you cast this. This card enters attached to that creature.)\nEnchanted creature has shroud. (It can't be the target of spells or abilities.)"),
}


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line', '') or declaration.get('oracle_text') != record.get('oracle_text')
            or 'variants' not in declaration):
        raise ValueError('tap damage requires a pinned normal single-face creature and reviewed variants')
    p = declaration.get('params')
    if not isinstance(p, dict) or set(p) != {'keywords', 'activation_cost', 'damage', 'targets', 'damage_you', 'damage_self', 'controller_life_loss', 'tap_target'}:
        raise ValueError('explicit tap damage amounts, cost, targets, and side effects required')
    if (any(type(p[k]) is not int or not 0 <= p[k] <= 4 for k in ('damage', 'damage_you', 'damage_self', 'controller_life_loss'))
            or not p['damage'] or type(p['tap_target']) is not bool
            or sum(bool(p[k]) for k in ('damage_you','damage_self','controller_life_loss','tap_target')) > 1):
        raise ValueError('bounded tap damage and one reviewed side effect required')
    cost = p['activation_cost']
    if not isinstance(cost, str) or not re.fullmatch(r'(?:(?:\{[0-9]+\}|\{[BUGRW]\})+, )?\{T\}', cost):
        raise ValueError('tap damage costs support only ordinary mana and tapping')
    activation = [0]*6 if cost == '{T}' else cost_from_oracle({'mana_cost':cost[:-5]})
    targets = {'any':'any target', 'creature':'target creature', 'flying_creature':'target creature with flying'}
    if p['targets'] not in targets: raise ValueError('unsupported tap damage target restriction')
    endings = ['.']
    if p['damage_you']: endings = [f" and {p['damage_you']} damage to you."]
    if p['damage_self']: endings = [f" and {p['damage_self']} damage to itself."]
    if p['controller_life_loss']: endings = [f" and that creature's controller loses {p['controller_life_loss']} life."]
    if p['tap_target']: endings = ['. Tap that creature.']
    if p['targets'] == 'any' and (p['controller_life_loss'] or p['tap_target']):
        raise ValueError('creature side effects require creature-only targets')
    refs = ('This creature', record['name'], record['name'].split(',')[0])
    expected = {f"{cost}: {ref} deals {p['damage']} damage to {targets[p['targets']]}{end}" for ref in refs for end in endings}
    keys = []; abilities = []; canonical = {k.lower():k for k in KEYWORDS}
    for line in record['oracle_text'].splitlines():
        if line in expected: abilities.append(line); continue
        text = line
        for reminder in re.findall(r'\([^\n]*?\)', line):
            if reminder not in REMINDERS: raise ValueError('unreviewed tap damage reminder')
            text = text.replace(reminder, '')
        tokens = [k.strip().lower() for k in text.strip().split(',')]
        if any(k not in canonical for k in tokens): raise ValueError('additional tap damage rules require dedicated cases')
        keys += [canonical[k] for k in tokens]
    if (len(abilities) != 1 or p['keywords'] != sorted(keys) or len(set(keys)) != len(keys)
            or set(keys) != set(record.get('keywords', [])) or set(keys) & {'Menace','Deathtouch','Trample','Lifelink'}):
        raise ValueError('tap damage parameters must match the reviewed whole text')
    for field in ('power','toughness'):
        if not re.fullmatch(r'\d+', record.get(field,'')) or not 0 <= int(record[field]) <= 20:
            raise ValueError('bounded numeric tap damage creature stats required')
    if int(record['toughness']) == 0: raise ValueError('tap damage source must survive entry')
    return dict(p, family='tap_damage_creature', activation_mana=activation, power=int(record['power']),
                toughness=int(record['toughness']), mana=cost_from_oracle(record))


def variants(p):
    result = creature_variants(p)+['survivor','lethal','friendly','opponent_controller','activation_payment',
        'tapped_activation','sick_activation','fresh_activation','opposing_turn','repeat_denied','cancel_target',
        'pending_snapshot','target_bounced','source_bounced','source_destroyed','target_destroyed',
        'cleanup_damage','cleanup_tapped','empty_zones']
    if sum(p['activation_mana']): result.append('activation_insufficient_mana')
    if any(p['activation_mana'][1:]): result.append('activation_wrong_color')
    if p['targets']=='any': result += ['opponent_player','self_player','self_target','indestructible']
    elif p['targets']=='creature': result += ['self_target','indestructible']
    elif 'Flying' not in p['keywords']: result += ['no_flying_targets']
    if p['damage_self'] < p['toughness']: result.append('untap_repeat')
    if p['targets'] != 'any': result.append('shroud_only_target')
    return result


def validate_witnesses(oracle, registry):
    for name, fields in WITNESSES.items():
        r = oracle.get(name,{})
        if name not in registry or r.get('layout') != 'normal' or tuple(r.get(k) for k in ('power','toughness','oracle_text')) != fields:
            raise ValueError('tap damage combat witness changed: '+name)
    for name, fields in SPELLS.items():
        if name not in registry or tuple(oracle.get(name,{}).get(k) for k in ('mana_cost','oracle_text')) != fields:
            raise ValueError('tap damage spell witness changed: '+name)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step, observe
    from creature_recipes import expand as creatures
    base = set(creature_variants(p))
    result = creatures(name,csvid,declaration,p,[v for v in selected if v in base],registry)
    for v in (v for v in selected if v not in base):
        if v == 'shroud_only_target':
            setup=[card(name,'source'),card('Robe of Mirrors','shroud_spell',zone='hand'),card('Unsummon','probe_spell',zone='hand')]
            actions=[step('choose_target',player=0,ref='source'),step('cast',ref='shroud_spell',value=1),step('resolve'),
                     step('assert_choices_consumed'),observe('source','in_play',1),observe('source','tapped',0),
                     step('can_cast',ref='probe_spell',value=0),step('can_activate',ref='source',value=0),step('assert_stack',value=0)]
            for player in (0,1):
                actions += [step('assert_life',player=player,value=20),step('assert_mana_total',player=player,value=59 if player==0 else 60),
                            step('assert_zone',player=player,zone='battlefield',value=2 if player==0 else 0)]
                for zone,names in (('hand',list(HAND)+(['Unsummon'] if player==0 else [])),('graveyard',GRAVE),('library',LIBRARY),('exile',EXILE)):
                    actions.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                    actions += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) if zone=='library'
                               else step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']) for i,n in enumerate(names)]
            result.append(dict(name=f'TapDamageCreatureRecipe_{csvid}_{v}',card=name,fixture='stocked_zones',seed=123,setup=setup,steps=actions,
                          _recipe=dict(family='tap_damage_creature',version=2,variant=v,params=declaration['params'],oracle_text=declaration['oracle_text'])))
            continue
        owner = int(v == 'opponent_controller'); target_owner = owner if v == 'friendly' else 1-owner
        player_target = v in ('opponent_player','self_player')
        if v == 'self_player': target_owner = owner
        target = ('Suntail Hawk' if v == 'lethal' else 'Air Elemental') if p['targets']=='flying_creature' else ('Muck Rats' if v == 'lethal' else 'Scaled Wurm')
        if v == 'indestructible': target = 'Darksteel Myr'
        if v == 'no_flying_targets': target = 'Scaled Wurm'
        fixture = 'empty_main' if v == 'empty_zones' else 'stocked_zones'
        hands = [list(HAND) if fixture == 'stocked_zones' else [] for _ in (0,1)]
        graves = [list(GRAVE) if fixture == 'stocked_zones' else [] for _ in (0,1)]
        battle = [0,0]; mana = [[10]*6,[10]*6]; life = [20,20]; source_damage = 0; target_damage = 0
        fresh = v == 'fresh_activation'; source_alive = True; target_alive = True
        setup = [card(name,'source',owner,'hand' if fresh else 'battlefield')]
        if not fresh: battle[owner] += 1
        if not player_target and v != 'self_target': setup.append(card(target,'recipient',target_owner)); battle[target_owner] += 1
        setup.append(card('Air Elemental' if p['targets']=='flying_creature' else 'Scaled Wurm','unselected',owner)); battle[owner] += 1
        # A flying-only shooter has no eligible target in the explicit negative board.
        if v == 'no_flying_targets': setup[-1]['name'] = 'Scaled Wurm'
        if v == 'tapped_activation': setup[0]['state'] = ['tapped']
        if v == 'sick_activation': setup[0]['state'] = ['sick']
        pre = []; run = []; extra = []
        if fresh:
            pre += [step('turn',player=owner,value=0),step('cast',ref='source',value=1),step('resolve')]
            mana[owner] = [mana[owner][i]-p['mana'][i] for i in range(6)];battle[owner] += 1
        if v == 'opposing_turn':pre.append(step('turn',player=1-owner,value=0))
        legal = v not in ('tapped_activation','activation_insufficient_mana','activation_wrong_color','no_flying_targets') and (v not in ('fresh_activation','sick_activation') or 'Haste' in p['keywords'])
        if v in ('activation_payment','activation_insufficient_mana','activation_wrong_color'):
            pool = list(p['activation_mana']) if v != 'activation_insufficient_mana' else [0]*6
            if v == 'activation_wrong_color':
                c=next(i for i in range(1,6) if pool[i]);pool[1 if c!=1 else 2]+=pool[c];pool[c]=0
            mana[owner]=pool;pre += [step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(pool)]
        def pay(cost, caster=owner):mana[caster][:]=[mana[caster][i]-cost[i] for i in range(6)]
        def apply_damage():
            nonlocal source_damage,target_damage,source_alive,target_alive
            if player_target:life[target_owner]-=p['damage']
            elif v=='self_target':source_damage+=p['damage'];life[owner]-=p['controller_life_loss']
            else:
                target_damage+=p['damage'];life[target_owner]-=p['controller_life_loss']
                if target!='Darksteel Myr' and target_damage>=int(oracle[target]['toughness']):
                    target_alive=False;battle[target_owner]-=1;graves[target_owner].append(target)
            life[owner]-=p['damage_you'];source_damage+=p['damage_self']
            if source_alive and source_damage>=p['toughness']:
                source_alive=False;battle[owner]-=1;graves[owner].append(name)
        ref = 'source' if v=='self_target' else 'recipient'
        def activation():
            choices=[step('choose_player',player=owner,value=target_owner)] if player_target else [step('choose_target',player=owner,ref=ref)]
            return [step('can_activate',ref='source',value=1)]+choices+[step('activate',ref='source',value=1),step('assert_stack',value=1),
                    observe('source','tapped',1),step('assert_choices_consumed')]
        if not legal:
            run += [step('can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
        elif v=='cancel_target':
            run += [step('can_activate',ref='source',value=1),step('choose_cancel_target',player=owner),step('activate',ref='source',value=0)]
        else:
            run += activation();pay(p['activation_mana'])
            # Costs happen on activation; the effect must wait for resolution.
            run += [step('assert_mana_total',player=owner,value=sum(mana[owner])),step('assert_life',player=0,value=20),step('assert_life',player=1,value=20),observe('source','damage',0)]
            if not player_target and v!='self_target':run.append(observe('recipient','damage',0))
            if v=='repeat_denied':run += [step('can_activate',ref='source',value=0),step('activate',ref='source',value=0)]
            if v in ('target_bounced','source_bounced'):
                setup.append(card('Unsummon','response',owner,'hand'));pay([0,0,1,0,0,0]);graves[owner].append('Unsummon')
                moved = 'recipient' if v=='target_bounced' else 'source'
                run += [step('choose_priority',ref='response'),step('choose_target',player=owner,ref=moved)]
                if v=='target_bounced':hands[target_owner].append(target);battle[target_owner]-=1;target_alive=False
                else:hands[owner].append(name);battle[owner]-=1;source_alive=False
            if v in ('source_destroyed','target_destroyed'):
                moved='source' if v=='source_destroyed' else 'recipient';run.append(step('destroy',ref=moved))
                if v=='source_destroyed':source_alive=False;battle[owner]-=1;graves[owner].append(name)
                else:target_alive=False;battle[target_owner]-=1;graves[target_owner].append(target)
            run += [step('resolve'),step('assert_choices_consumed')]
            if v not in ('target_bounced','target_destroyed'):
                apply_damage()
            if v=='untap_repeat':
                setup.append(card('Vitalize','untap_spell',owner,'hand'));pay([0,0,0,1,0,0]);graves[owner].append('Vitalize')
                run += [step('cast',ref='untap_spell',value=1),step('resolve'),observe('source','tapped',0)]+activation()+[step('resolve')]
                pay(p['activation_mana']);apply_damage()
        checks = [step('assert_stack',value=0),step('assert_choices_consumed'),observe('source','in_play',int(source_alive)),
                  observe('unselected','in_play',1),observe('unselected','damage',0)]
        if source_alive:
            checks += [observe('source','tapped',int(legal and v!='cancel_target' or v=='tapped_activation')),
                       observe('source','power',p['power']),observe('source','toughness',p['toughness']),observe('source','damage',source_damage)]
        if not player_target and v!='self_target':
            checks.append(observe('recipient','in_play',int(target_alive)))
            if target_alive:
                checks += [observe('recipient','damage',target_damage),observe('recipient','tapped',int(legal and v!='cancel_target' and p['tap_target']))]
        for player in (0,1):
            checks += [step('assert_life',player=player,value=life[player]),step('assert_mana_total',player=player,value=sum(mana[player])),step('assert_zone',player=player,zone='battlefield',value=battle[player])]
            if v=='activation_payment':checks += [step('assert_mana',player=player,color=c,value=n) for c,n in enumerate(mana[player])]
            for zone,names in (('hand',hands[player]),('graveyard',graves[player]),('library',LIBRARY if fixture=='stocked_zones' else []),('exile',EXILE if fixture=='stocked_zones' else [])):
                checks.append(step('assert_zone',player=player,zone=zone,value=len(names)))
                checks += [step('assert_zone_card',player=player,zone=zone,index=i,value=registry[n]['csvid']) if zone=='library'
                           else step('assert_zone_contains',player=player,zone=zone,value=registry[n]['csvid']) for i,n in enumerate(names)]
        if v in ('cleanup_damage','cleanup_tapped'):
            checks += [step('cleanup'),observe('recipient','damage',0)]
            if source_alive:checks += [observe('source','damage',0),observe('source','tapped',1),step('can_activate',ref='source',value=0)]
        actions=pre+run+checks
        if v=='pending_snapshot':
            cut=next(i for i,s in enumerate(run) if s['op']=='resolve')
            actions=pre+run[:cut]+[step('capture')]+run[cut:]+checks+[step('record_state'),step('ai_roundtrip'),step('restore')]+run[cut:]+checks+[step('assert_recorded_state')]
        result.append(dict(name=f'TapDamageCreatureRecipe_{csvid}_{v}',card=name,fixture=fixture,seed=123,setup=setup,steps=actions,
                      _recipe=dict(family='tap_damage_creature',version=2,variant=v,params=declaration['params'],oracle_text=declaration['oracle_text'],untested_keywords=['Flash'] if 'Flash' in p['keywords'] else [])))
    return result
