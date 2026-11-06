"""Wave-six full-pin mana spells and opt-in permanent mana models.

Old mana declarations never enter this module.  Mana abilities finish during
activation; spells retain their real pending/resolution boundary.
"""
import copy
import json
import re
from collections import Counter
from pathlib import Path

SYMBOLS = 'CBUGRW'
PIN_PATH = Path(__file__).parent / 'data/mana_effects_extension_witnesses.json'
FAMILIES = {'mana_spell_extension', 'mana_modes', 'mana_consequence', 'permanent_mana', 'mana_creature'}


def pins():
    return json.loads(PIN_PATH.read_text(encoding='utf8'))


def applies(d):
    return isinstance(d.get('params'), dict) and d['params'].get('wave') == 6


def _same(a, b):
    """JSON structure equality must distinguish bools from integer costs."""
    if type(a) is not type(b): return False
    if isinstance(a, dict): return a.keys() == b.keys() and all(_same(a[k], b[k]) for k in a)
    if isinstance(a, list): return len(a) == len(b) and all(_same(x, y) for x, y in zip(a, b))
    return a == b


def spec(d, o):
    from recipes import cost_from_oracle
    if (not o or type(d.get('version')) is not int or d.get('version') != 2 or o.get('layout') != 'normal'
            or o.get('card_faces') or o.get('selected_face') or d.get('oracle_text') != o.get('oracle_text')
            or not isinstance(d.get('variants'), list) or d.get('family') not in FAMILIES):
        raise ValueError('whole normal Oracle, version2 and explicit variants required')
    profile = pins()['sources'].get(o['name'])
    if (not profile or not _same(d.get('params'), profile['params'])
            or any(o.get(k) != v for k, v in profile['oracle'].items())):
        raise ValueError('reviewed full source and exact wave6 profile required')
    p = copy.deepcopy(profile['params'])
    base_keys={'wave', 'model', 'family', 'modes', 'enters_tapped', 'depletion', 'gaps'}
    if set(p) not in (base_keys,base_keys|{'fixture'}) or p.get('fixture','stocked_zones') not in ('stocked_zones','empty_main'):
        raise ValueError('explicit wave6 model fields required')
    if (type(p['wave']) is not int or p['wave'] != 6 or p['family'] != d['family']
            or p['model'] not in ('spell', 'permanent') or type(p['enters_tapped']) is not bool
            or type(p['depletion']) is not int or p['depletion'] not in (0, 2)
            or not isinstance(p['gaps'], list) or any(not isinstance(x, str) for x in p['gaps'])):
        raise ValueError('typed mana model required')
    if not isinstance(p['modes'], list) or not 1 <= len(p['modes']) <= 3:
        raise ValueError('bounded explicit mana modes required')
    for mode in p['modes']:
        base={'pool', 'count', 'input', 'tap', 'sacrifice', 'condition', 'dialog'}
        if (set(mode) not in (base,base|{'consequence'})
                or any(not isinstance(mode[k], list) or len(mode[k]) != 6 or any(type(n) is not int or not 0 <= n <= 10 for n in mode[k]) for k in ('pool', 'input'))
                or not any(mode['pool'])
                or mode['count'] not in ('fixed','own_creatures','all_goblins','threshold7','source_power','own_artifacts','own_enchantments','own_swamps','own_forests','all_elves','foe_tapped_lands','own_grave_creatures','rite_flame','foe_grave_union') or mode['condition'] not in ('none','own_swamp','own_island','own_plains','lands5','power4','total8')
                or type(mode['tap']) is not bool or type(mode['sacrifice']) is not bool
                or mode['dialog'] not in (None, 0, 1) or type(mode['dialog']) is bool):
            raise ValueError('exact output, input, predicate and cost components required')
        if 'consequence' in mode:
            c=mode['consequence']
            if not isinstance(c,dict) or set(c)!={'kind','amount'} or c['kind'] not in ('life_cost','self_damage','settled_opponent_trigger') or type(c['amount']) is not int or c['amount']!=1:
                raise ValueError('reviewed life payment or preventable damage required')
    is_spell = 'Instant' in o['type_line'] or 'Sorcery' in o['type_line']
    if (p['model'] == 'spell') != is_spell or is_spell != (p['family'] == 'mana_spell_extension'):
        raise ValueError('spell and immediate mana source families differ')
    p.update(mana=None if 'Land' in o['type_line'] else cost_from_oracle(o),
             creature='Creature' in o['type_line'], land='Land' in o['type_line'],
             power=int(o.get('power', '0')), toughness=int(o.get('toughness', '0')))
    return p


def variants(p):
    common = ['effect', 'payment', 'insufficient_cast_mana', 'ready_snapshot', 'resolved_snapshot']
    if p['model'] == 'spell':
        common += ['pending_snapshot', 'countered', 'spend_colored', 'spend_generic']
        if p['modes'][0]['count'] in ('own_creatures','all_goblins'):
            common += ['zero_count', 'opponent_controlled', 'response_loss']
        if p['modes'][0]['count']=='threshold7':common+=['grave_six','grave_seven','grave_eight','opponent_grave_seven']
        if p['modes'][0]['count'] in ('foe_tapped_lands','own_grave_creatures','foe_grave_union'):common+=['zero_count','response_loss']
        if p['modes'][0]['count']=='rite_flame':common+=['no_grave_copies','opponent_grave_copies','two_copies']
        if p['modes'][0]['count']=='foe_grave_union':common+=['opponent_shroud']
    else:
        common = ['entry', 'tapped', 'sick', 'incidental_tap', 'source_bounced', 'tax', 'ready_snapshot', 'resolved_snapshot']
        if not p['land']:
            common += ['fresh_cast', 'payment', 'insufficient_cast_mana']
        if p['depletion']:
            common += ['exhaustion']
        if any(m['condition'] != 'none' for m in p['modes']):
            common += ['absent_condition', 'opposing_condition', 'condition_removed']
        if p['modes'][0]['count']=='own_creatures':common+=['zero_count','opponent_controlled','count_removed']
        if p['modes'][0]['count']=='source_power':common+=['pumped']
        if p['modes'][0]['count'] in ('own_artifacts','own_enchantments','own_swamps','own_forests'):common+=['zero_count','count_removed']
        if p['modes'][0]['count']=='all_elves':common+=['opponent_elves','count_removed']
        if any('consequence' in m for m in p['modes']):common+=['prevention']
        for i in range(len(p['modes'])):
            common += [f'effect_m{i}', f'spend_generic_m{i}']
            if any(p['modes'][i]['input']):
                common += [f'payment_m{i}', f'insufficient_input_m{i}']
                if any(p['modes'][i]['input'][1:]): common += [f'wrong_color_input_m{i}']
            if p['modes'][i]['dialog'] == 1 and not p['modes'][i]['tap'] and p['modes'][i]['sacrifice']:
                common += [f'ready_snapshot_m{i}']
            if p['modes'][i]['tap'] and not p['modes'][i]['sacrifice']:
                repeat='tap_then_sacrifice' if any(not m['tap'] and m['sacrifice'] for m in p['modes']) else 'repeat_denied'
                common += [f'{repeat}_m{i}', f'untap_repeat_m{i}']
    result = []
    for owner in (0, 1):
        for v in common:
            indexed = re.search(r'_m(\d+)$', v)
            i = int(indexed[1]) if indexed else 0
            # Avoid reproducing the already isolated AI zero-effect policy on
            # every source sharing that helper; human zero remains exercised.
            if owner==1 and v=='zero_count' and p['modes'][i]['count'] in ('own_artifacts','own_enchantments'):continue
            # sac_land's optional sacrifice menu does not exist on its AI path.
            if owner == 1 and p['land'] and p['modes'][i]['sacrifice'] and p['modes'][i]['dialog'] is not None:
                continue
            result.append(v + '_' + str(owner))
    return result


def validate_witnesses(o, registry):
    for name, record in pins()['witnesses'].items():
        row = registry.get(name)
        if (not row
                or any(o.get(name, {}).get(k) != v for k, v in record.items())):
            raise ValueError('full independent mana witness changed: ' + name)


def expand(name, csvid, d, p, selected, registry, o):
    from recipes import card, step, observe, cost_from_oracle
    from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
    validate_witnesses(o, registry)
    if not selected or len(set(selected)) != len(selected) or any(v not in variants(p) for v in selected):
        raise ValueError('unique reviewed mana scenarios required')
    result = []
    for variant in selected:
        v, owner = variant.rsplit('_', 1); owner = int(owner); enemy = 1-owner
        indexed = re.search(r'_m(\d+)$', v)
        index = int(indexed[1]) if indexed else 0
        if indexed: v = v[:indexed.start()]
        if v in ('tapped', 'incidental_tap'):
            index = next((i for i,m in enumerate(p['modes']) if not m['tap']), index)
        mode = p['modes'][index]
        fixture=p.get('fixture','stocked_zones')
        zones = {z: [list(xs) if fixture=='stocked_zones' else [], list(xs) if fixture=='stocked_zones' else []] for z, xs in [('hand', HAND), ('graveyard', GRAVE), ('library', LIBRARY), ('exile', EXILE)]}
        mana = [[0]*6, [0]*6]; life = [20, 20]; entities = {}; setup = []
        cast_source = p['model'] == 'spell' or not indexed and v in ('fresh_cast', 'payment', 'insufficient_cast_mana') and not p['land']
        def add(nm, ref, pl, zone='battlefield', states=None):
            c = card(nm, ref, pl, zone)
            if states: c['state'] = states
            setup.append(c); r = o[nm]
            entities[ref] = dict(name=nm, player=pl, origin_zone=zone, alive=zone == 'battlefield', tapped=bool(states and 'tapped' in states), sick=bool(states and 'sick' in states), damage=0)
            if zone != 'battlefield': zones[zone][pl].append(nm)
        def pool(pl, values):
            mana[pl] = list(values)
            return [step('set_mana', player=pl, color=i, value=n) for i, n in enumerate(values)] + [step('refresh')]
        def receipt():
            seq = [step('assert_stack', value=0), step('assert_choices_consumed')]
            for pl in (0, 1):
                seq += [step('assert_mana', player=pl, color=i, value=n) for i, n in enumerate(mana[pl])]
                seq += [step('assert_life', player=pl, value=life[pl])]
            for ref, e in entities.items():
                # Grave/library/exile setup refs are zone indexes, not active
                # card slots. Their identities are certified by zone receipts.
                if e['origin_zone'] not in ('battlefield','hand'):continue
                seq += [observe(ref, 'in_play', int(e['alive']))]
                if e['alive']:
                    seq += [observe(ref, 'tapped', int(e['tapped']))]
                    if 'Creature' in o[e['name']]['type_line']:
                        seq += [observe(ref, 'power', int(o[e['name']]['power'])+e.get('boost',0)), observe(ref, 'toughness', int(o[e['name']]['toughness'])+e.get('boost',0)), observe(ref, 'damage', e['damage'])]
            return seq
        def consume(ref, destination):
            e = entities[ref]; e['alive'] = False; zones[destination][e['player']].append(e['name'])
        def aux(nm, ref, pl, target=None, dialog=None):
            add(nm, ref, pl, 'hand'); values = cost_from_oracle(o[nm]); seq = pool(pl, values)
            if target: seq += [step('choose_target', player=pl, ref=target)]
            seq += [step('cast', ref=ref, value=1), step('assert_stack', value=1)]
            if dialog is not None: seq += [step('choose_dialog', player=pl, value=dialog)]
            seq += [step('resolve'), step('assert_choices_consumed')]
            mana[pl] = [0]*6; zones['hand'][pl].remove(nm)
            if 'Instant' in o[nm]['type_line'] or 'Sorcery' in o[nm]['type_line']: zones['graveyard'][pl].append(nm)
            else: entities[ref]['alive'] = True
            return seq
        states = ['tapped'] if v == 'tapped' else ['sick'] if v == 'sick' else None
        add(name, 'source', owner, 'hand' if cast_source else 'battlefield', states)
        if not cast_source and p['enters_tapped']: entities['source']['tapped'] = True
        add('Sol Ring', 'artifact_control', enemy)
        if mode['count'] in ('own_creatures','all_goblins'):
            if v not in ('zero_count', 'opponent_controlled'):
                nm="Mons's Goblin Raiders" if mode['count']=='all_goblins' else 'Grizzly Bears'
                add(nm, 'first', owner); add(nm, 'second', owner)
            if mode['count']=='all_goblins':
                if v!='zero_count':add("Mons's Goblin Raiders",'opponent_goblin',enemy)
                add('Grizzly Bears','wrong_tribe',owner)
            else:add('Hill Giant', 'opponent', enemy)
        if mode['count']=='threshold7':
            target=7 if v=='grave_seven' else 8 if v=='grave_eight' else 6
            for pl,total in [(owner,target)]+([(enemy,7)] if v=='opponent_grave_seven' else []):
                for i in range(total-len(zones['graveyard'][pl])):add('Muck Rats',f'threshold_{pl}_{i}',pl,'graveyard')
        if mode['count'] in ('own_artifacts','own_enchantments','own_swamps','own_forests','all_elves'):
            nm={'own_artifacts':'Phyrexian Hulk','own_enchantments':'Megrim','own_swamps':'Swamp','own_forests':'Forest','all_elves':'Elvish Mystic'}[mode['count']]
            if v not in ('zero_count','opponent_elves'):
                for j in range(2):add(nm,'first' if j==0 else 'second',owner,states=['tapped'] if 'Land' in o[nm]['type_line'] or mode['count']=='all_elves' and v=='insufficient_cast_mana' else None)
            add(nm,'opposing_count',enemy,states=['tapped'] if 'Land' in o[nm]['type_line'] else None)
            add('Grizzly Bears','wrong_count',owner)
        if mode['count']=='foe_tapped_lands':
            if v!='zero_count':
                add('Forest','first',enemy,states=['tapped']);add('Forest','second',enemy,states=['tapped'])
            add('Island','untapped_land',enemy);add('Forest','own_land',owner,states=['tapped']);add('Grizzly Bears','tapped_creature',enemy,states=['tapped'])
        if mode['count'] in ('own_grave_creatures','foe_grave_union'):
            target=owner if mode['count']=='own_grave_creatures' else enemy
            if v!='zero_count':
                for j,nm in enumerate(['Muck Rats','Phyrexian Hulk']+(['Sol Ring'] if mode['count']=='foe_grave_union' else [])):add(nm,f'grave_count_{j}',target,'graveyard')
            add('Forest','grave_land',target,'graveyard');add('Muck Rats','wrong_grave',1-target,'graveyard')
            if v=='response_loss':add('Grizzly Bears','first',target)
        if mode['count']=='rite_flame':
            for pl,n in [(owner,0 if v in ('no_grave_copies','opponent_grave_copies') else 1),(enemy,0 if v=='no_grave_copies' else 2)]:
                for j in range(n):add(name,f'rite_grave_{pl}_{j}',pl,'graveyard')
            if v=='two_copies':add(name,'second_rite',owner,'hand')
        if v=='opponent_shroud':add('Ivory Mask','player_shroud',enemy)
        def output_count(selected_mode=None):
            kind=(selected_mode or mode)['count']
            if kind=='fixed':return 1
            if kind=='threshold7':return 5 if len(zones['graveyard'][owner])>=7 else 3
            if kind=='source_power':return p['power']+entities['source'].get('boost',0)
            if kind=='all_goblins':return sum(e['alive'] and 'Goblin' in o[e['name']]['type_line'].split(' — ')[-1].split() for e in entities.values())
            if kind=='all_elves':return sum(e['alive'] and 'Elf' in o[e['name']]['type_line'].split(' — ')[-1].split() for e in entities.values())
            if kind in ('own_artifacts','own_enchantments'):return sum(e['alive'] and e['player']==owner and ('Artifact' if kind=='own_artifacts' else 'Enchantment') in o[e['name']]['type_line'] for e in entities.values())
            if kind in ('own_swamps','own_forests'):return sum(e['alive'] and e['player']==owner and ('Swamp' if kind=='own_swamps' else 'Forest') in o[e['name']]['type_line'].split(' — ')[-1].split() for e in entities.values())
            if kind=='foe_tapped_lands':return sum(e['alive'] and e['player']==enemy and e['tapped'] and 'Land' in o[e['name']]['type_line'] for e in entities.values())
            if kind=='own_grave_creatures':return sum('Creature' in o[nm]['type_line'] for nm in zones['graveyard'][owner])
            if kind=='foe_grave_union':return sum(any(t in o[nm]['type_line'] for t in ('Artifact','Creature')) for nm in zones['graveyard'][enemy])
            if kind=='rite_flame':return 2+sum(zones['graveyard'][pl].count(name) for pl in (0,1))
            return sum(e['alive'] and e['player']==owner and 'Creature' in o[e['name']]['type_line'] for e in entities.values())
        conditional = any(m['condition'] != 'none' for m in p['modes'])
        if any(m['condition']=='own_swamp' for m in p['modes']):
            if v != 'absent_condition': add('Swamp', 'condition', enemy if v == 'opposing_condition' else owner)
            add('Plains', 'wrong_land', owner)
        elif conditional:
            conditions={m['condition'] for m in p['modes']}
            pl=enemy if v=='opposing_condition' else owner
            absent=v=='absent_condition'
            if conditions & {'own_island','own_plains'}:
                if not absent:add('Island','condition',pl);add('Plains','condition_plains',pl)
                add('Forest','wrong_land',owner)
            elif 'lands5' in conditions:
                for j in range(3 if absent else 4):add('Forest','condition' if j==0 else 'condition_'+str(j),pl,states=['tapped'])
                add('Grizzly Bears','wrong_land',owner)
            elif 'power4' in conditions:
                # Bind the printed ordinary mode below Ferocious rather than
                # forcing a menu the handler never offers when Ferocious holds.
                if not absent and (index!=0 or v in ('condition_removed','opposing_condition')):add('Air Elemental','condition',pl)
                add('Hill Giant','below_individual',owner);add('Muck Rats','power_one',owner)
            elif 'total8' in conditions:
                for j in range(2 if absent else 3):add('Hill Giant' if absent and j==1 else 'Grizzly Bears','condition' if j==0 else 'condition_'+str(j),pl)
        def predicate(condition):
            if condition=='none':return True
            live=[e for e in entities.values() if e['alive'] and e['player']==owner]
            if condition.startswith('own_'):return any({'own_swamp':'Swamp','own_island':'Island','own_plains':'Plains'}[condition] in o[e['name']]['type_line'].split(' — ')[-1].split() for e in live)
            if condition=='lands5':return sum('Land' in o[e['name']]['type_line'] for e in live)>=5
            powers=[int(o[e['name']]['power'])+e.get('boost',0) for e in live if 'Creature' in o[e['name']]['type_line']]
            return any(n>=4 for n in powers) if condition=='power4' else sum(powers)>=8
        pre = [step('turn', player=owner, value=0)] + pool(owner, [0]*6) + pool(enemy, [0]*6)
        pre += [observe('source', 'in_hand' if cast_source else 'in_play', 1)]
        if cast_source:
            funded = list(p['mana'])
            if v == 'insufficient_cast_mana': funded = [0]*6
            pre += pool(owner, funded)
        if p['model'] == 'spell':
            actions = []
            if v == 'opponent_shroud':
                actions += [step('require_can_cast',ref='source',value=0),step('assert_stack',value=0),observe('source','in_hand',1)]
            elif v == 'insufficient_cast_mana':
                actions += [step('cast', ref='source', value=0), step('assert_stack', value=0), observe('source', 'in_hand', 1)]
            else:
                start = [step('cast', ref='source', value=1), step('assert_stack', value=1)]
                mana[owner] = [0]*6; zones['hand'][owner].remove(name)
                start += [step('assert_mana', player=owner, color=i, value=0) for i in range(6)]
                actions += start
                if v == 'countered':
                    add('Negate', 'counter', enemy, 'hand'); actions += pool(enemy, cost_from_oracle(o['Negate']))
                    # The real counterspell context binds its sole pending parent.
                    actions += [step('cast_response', ref='counter', value=1), step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
                    mana[enemy] = [0]*6; zones['hand'][enemy].remove('Negate'); zones['graveyard'][enemy] += ['Negate', name] if owner == enemy else ['Negate']
                    zones['graveyard'][owner].append(name)
                else:
                    if v == 'response_loss':
                        response='Boomerang' if mode['count']=='foe_tapped_lands' else 'Murder'
                        add(response, 'response', enemy, 'hand'); actions += pool(enemy, cost_from_oracle(o[response]))
                        actions += [step('choose_target', player=enemy, ref='first'), step('cast_response', ref='response', value=1), step('assert_stack', value=2), step('resolve_response'), step('assert_choices_consumed')]
                        mana[enemy] = [0]*6; zones['hand'][enemy].remove(response); zones['graveyard'][enemy].append(response); consume('first', 'hand' if response=='Boomerang' else 'graveyard')
                    # resolve_response drains the real response and its parent.
                    if v != 'response_loss': actions += [step('resolve')]
                    actions += [step('assert_choices_consumed')]
                    count = output_count()
                    mana[owner] = [n*count for n in mode['pool']]; zones['graveyard'][owner].append(name)
                    if mode['count']=='foe_grave_union':life[owner]+=count
                    if v=='two_copies':
                        actions += [step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner])]
                        actions += [step('cast',ref='second_rite',value=1),step('assert_stack',value=1)]
                        mana[owner]=[a-b for a,b in zip(mana[owner],p['mana'])];zones['hand'][owner].remove(name)
                        actions += [step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(mana[owner])]
                        actions += [step('resolve'),step('assert_choices_consumed')]
                        n=output_count();mana[owner]=[a+b*n for a,b in zip(mana[owner],mode['pool'])];zones['graveyard'][owner].append(name)
                actions += receipt()
        else:
            actions = []
            if cast_source:
                if v == 'insufficient_cast_mana':
                    actions += [step('cast', ref='source', value=0), observe('source', 'in_hand', 1)]
                else:
                    actions += [step('cast', ref='source', value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]
                    zones['hand'][owner].remove(name); entities['source']['alive'] = True; entities['source']['sick'] = p['creature']; mana[owner] = [0]*6
                actions += receipt()
                if v == 'fresh_cast':
                    if any(mode['input']): actions += pool(owner,mode['input'])
                    actions += [step('require_can_activate', ref='source', value=int(not p['creature'] or not mode['tap']))]
            elif v == 'entry': actions += receipt()
            else:
                if entities['source']['tapped'] and v != 'tapped':
                    actions += aux('Twiddle', 'entry_untap', owner, 'source', 1); entities['source']['tapped'] = False
                if v == 'incidental_tap':
                    actions += aux('Twiddle', 'incidental', enemy, 'source', 0); entities['source']['tapped'] = True
                if v == 'source_bounced':
                    actions += aux('Boomerang', 'bounce', enemy, 'source'); consume('source', 'hand')
                elif v == 'condition_removed':
                    actions += aux('Boomerang', 'bounce', enemy, 'condition'); consume('condition', 'hand')
                if v == 'tax': actions += aux('Suppression Field', 'taxer', enemy)
                if v == 'count_removed':
                    removal='Murder' if mode['count'] in ('own_creatures','all_elves') else 'Disenchant' if mode['count'] in ('own_artifacts','own_enchantments') else 'Boomerang'
                    actions+=aux(removal,'remove_count',enemy,'first');consume('first','hand' if removal=='Boomerang' else 'graveyard')
                if v == 'pumped':actions+=aux('Giant Growth','pump',owner,'source');entities['source']['boost']=3
                if v == 'prevention':actions+=aux('Safe Passage','prevent',enemy if mode.get('consequence',{}).get('kind')=='settled_opponent_trigger' else owner)
                # Ramos's AI menu is absent while untapped, but its printed
                # nontapping sacrifice mode remains available after a real tap.
                if owner == 1 and not p['land'] and mode['dialog'] == 1 and mode['sacrifice'] and not mode['tap'] and not entities['source']['tapped']:
                    actions += aux('Twiddle','bind_sacrifice',owner,'source',0);entities['source']['tapped']=True
                funding=list(mode['input'])
                if v=='insufficient_input':funding=[0]*6
                if v=='wrong_color_input':
                    funding=[sum(mode['input']),0,0,0,0,0]
                actions += pool(owner, funding)
                legal = v not in ('insufficient_input','wrong_color_input') and predicate(mode['condition']) and entities['source']['alive'] and (not mode['tap'] or not entities['source']['tapped'] and not (p['creature'] and entities['source']['sick']))
                if v == 'source_bounced':
                    actions += [observe('source', 'in_play', 0)]
                elif not legal:
                    actions += [step('require_can_activate', ref='source', value=0), step('activate', ref='source', value=0), step('assert_choices_consumed')]
                else:
                    def activation(exhaust=False, selected_mode=None):
                        active_mode=selected_mode or mode
                        seq = [step('require_can_activate', ref='source', value=1)]
                        if active_mode['dialog'] is not None and owner == 0 and not entities['source']['tapped']: seq += [step('choose_dialog', player=owner, value=active_mode['dialog'])]
                        colors={i for m in p['modes'] if predicate(m['condition']) for i,n in enumerate(m['pool']) if n}
                        if conditional and len(colors)>1: seq += [step('choose_color', player=owner, value=next(i for i,n in enumerate(active_mode['pool']) if n))]
                        seq += [step('activate', ref='source', value=1), step('assert_choices_consumed'), step('assert_stack', value=0)]
                        factor=output_count(active_mode)
                        mana[owner] = [a-b+c*factor for a,b,c in zip(mana[owner], active_mode['input'], active_mode['pool'])]
                        consequence=active_mode.get('consequence')
                        if consequence:
                            if consequence['kind']=='life_cost' or v!='prevention':life[enemy if consequence['kind']=='settled_opponent_trigger' else owner]-=consequence['amount']
                        entities['source']['tapped'] = active_mode['tap']
                        if active_mode['sacrifice'] or exhaust: consume('source', 'graveyard')
                        return seq + receipt()
                    actions += activation()
                    if v == 'repeat_denied': actions += [step('require_can_activate', ref='source', value=0)]
                    if v == 'tap_then_sacrifice':actions+=activation(selected_mode=next(m for m in p['modes'] if not m['tap'] and m['sacrifice']))
                    if v in ('untap_repeat', 'exhaustion'):
                        actions += aux('Twiddle', 'untap_second', owner, 'source', 1); entities['source']['tapped'] = False
                        actions += pool(owner, mode['input'])
                        actions += activation(exhaust=bool(p['depletion']))
        # Spend produced mana on real spells.  Keep the exact production receipt first.
        if v == 'spend_generic':
            produced = sum(mana[owner]); needed = max(0, 6-produced)
            mana[owner][0] += needed; actions += [step('set_mana', player=owner, color=0, value=mana[owner][0])]
            add('Phyrexian Hulk', 'spender', owner, 'hand')
            if owner == 0: actions += [step('choose_mana', player=owner, color=i) for i,n in enumerate(mana[owner]) if i for _ in range(n)]
            actions += [step('cast', ref='spender', value=1), step('assert_stack', value=1), step('resolve'), step('assert_choices_consumed')]
            mana[owner] = [0]*6; zones['hand'][owner].remove('Phyrexian Hulk'); entities['spender']['alive'] = True
        if v == 'spend_colored':
            spenders = {1:'Muck Rats', 2:'Merfolk of the Pearl Trident', 3:'Elvish Mystic', 4:"Mons's Goblin Raiders", 5:'Savannah Lions'}
            for color in range(1,6):
                for j in range(mana[owner][color]):
                    nm=spenders[color];ref=f'spend_{color}_{j}';add(nm,ref,owner,'hand')
                    actions += [step('cast',ref=ref,value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
                    mana[owner][color]-=1;zones['hand'][owner].remove(nm);entities[ref]['alive']=True
        checks = receipt(); universe = set(HAND+GRAVE+EXILE+LIBRARY) | {e['name'] for e in entities.values()}
        for pl in (0,1):
            for zone in ('battlefield','hand','graveyard','library','exile'):
                names = [e['name'] for e in entities.values() if e['alive'] and e['player']==pl] if zone=='battlefield' else zones[zone][pl]
                checks += [step('assert_zone',player=pl,zone=zone,value=len(names))]
                if zone=='library': checks += [step('assert_zone_card',player=pl,zone=zone,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(names)]
                else:
                    counts=Counter(names);checks += [step('assert_zone_multiplicity',player=pl,zone=zone,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
        steps = pre + actions + checks
        if v == 'ready_snapshot': steps = pre+[step('capture')]+copy.deepcopy(actions+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions+checks)+[step('assert_recorded_state')]
        if v == 'resolved_snapshot': steps += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        if v == 'pending_snapshot': steps = pre+start+[step('capture')]+copy.deepcopy(actions[len(start):]+checks)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(actions[len(start):]+checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'ManaWave6Recipe_{csvid}_{variant}',card=name,fixture=fixture,seed=123,setup=setup,steps=steps,
                           _recipe=dict(family=d['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_components=p['gaps'])))
    return result
