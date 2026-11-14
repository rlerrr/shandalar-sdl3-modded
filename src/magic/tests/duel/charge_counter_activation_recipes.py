"""Whole-Oracle charge additions: real costs, targets and resolution timing."""
import copy
import json
import re
from collections import Counter
from pathlib import Path

from permanent_value_activation_recipes import parse_cost, mana, number

FAMILY = 'charge_counter_activation'
DATA = Path(__file__).parent / 'data/charge_counter_activation_wave8_profiles.json'


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def parse(record, ability):
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('type_line', '').startswith('Token ')
            or not any(t in record.get('type_line', '') for t in ('Artifact', 'Creature', 'Land'))):
        raise ValueError('whole normal artifact, creature or land required')
    lines = record['oracle_text'].splitlines()
    if type(ability) is not int or not 0 <= ability < len(lines) or ': ' not in lines[ability]:
        raise ValueError('explicit printed ability line required')
    cost_text, effect = lines[ability].split(': ', 1)
    match = re.fullmatch(r'Put (a|two|X) charge counters? on (this artifact|this land|target artifact)\.( Activate only as a sorcery\.)?', effect)
    if not match:
        raise ValueError('complete bounded charge addition effect required')
    cost = parse_cost(cost_text)
    if (cost['sacrifice'] not in (None, 'self', 'creature') or cost['discard'] or cost['pay_life'] or 'loyalty' in cost
            or cost.get('counter', {}).get('kind', 'charge') != 'charge'):
        raise ValueError('unpiloted charge activation cost')
    amount = number(match[1])
    if (amount == 'X') != cost['x']:
        raise ValueError('chosen X must occur in both printed cost and effect')
    initial = 0
    for line in lines:
        entry = re.fullmatch(r'This artifact enters with (a|one|two|three|four|five|six) charge counters? on it\.', line)
        if entry:
            initial = number(entry[1])
    if 'counter' in cost and initial < cost['counter']['amount']:
        raise ValueError('actual printed entry must supply the charge cost')
    creature = 'Creature' in record['type_line']
    if creature and any(not re.fullmatch(r'[1-9][0-9]?', record.get(prop, '')) for prop in ('power', 'toughness')):
        raise ValueError('surviving numeric source creature required')
    return dict(family=FAMILY, ability=ability, cost=cost, amount=amount, target=match[2] == 'target artifact',
                sorcery=bool(match[3]), initial=initial, creature=creature, land='Land' in record['type_line'],
                activation_mana=mana(cost['mana']), casting_mana=mana(record['mana_cost']),
                gaps=[line for i, line in enumerate(lines) if i != ability])


def spec(declaration, record):
    if declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2:
        raise ValueError('charge activation family and integer version2 required')
    params = declaration.get('params')
    if (not isinstance(params, dict) or set(params) != {'ability', 'interface', 'inactive_clauses'}
            or declaration.get('oracle_text') != (record or {}).get('oracle_text')):
        raise ValueError('whole Oracle and explicit ability/interface/gaps required')
    p = parse(record, params['ability'])
    source = pins()['sources'].get(record['name'], {})
    if (not source or any(record.get(k) != v for k, v in source['oracle'].items())
            or params['inactive_clauses'] != p['gaps']
            or source['abilities'].get(str(p['ability'])) != params['interface']):
        raise ValueError('independent whole source or interface pin differs')
    ui = params['interface']
    required = {'dialogs', 'intent', 'sacrifice_choices', 'disabled_tap_mode', 'mana_shortage_query_only', 'exact_cost_negatives'}
    optional = {'mana_sibling_x_domain', 'disabled_cost_modes'}
    if not required <= set(ui) or set(ui) - required - optional:
        raise ValueError('complete scoped charge interface required')
    for key in ('disabled_tap_mode', 'mana_shortage_query_only', 'exact_cost_negatives'):
        if type(ui[key]) is not bool:
            raise ValueError('typed charge scenario annotations required')
    if (not isinstance(ui['dialogs'], dict) or any(k not in ('0', '1') or type(v) is not int or not 0 <= v <= 16 for k, v in ui['dialogs'].items())
            or not isinstance(ui['sacrifice_choices'], dict)
            or any(k not in ('0', '1') or type(v) is not bool for k, v in ui['sacrifice_choices'].items())):
        raise ValueError('typed per-controller real mode/cost inputs required')
    if ui['intent'] is not None:
        intent = ui['intent']
        if (not isinstance(intent, dict) or set(intent) != {'mode_label', 'fallback_color', 'allow_auto_nonmana'}
                or not isinstance(intent['mode_label'], str) or not 1 <= len(intent['mode_label'].encode('utf8')) <= 127
                or any(c in intent['mode_label'] for c in '\r\n') or type(intent['fallback_color']) is not int
                or not 0 <= intent['fallback_color'] <= 6 or intent['allow_auto_nonmana'] is not True or ui['dialogs']):
            raise ValueError('exact unique-nonmana input intent required')
        siblings = [line.split(': ', 1)[1] for line in p['gaps'] if ': ' in line]
        if (not siblings or any(not effect.startswith('Add ') for effect in siblings)
                or any(': ' not in line for line in p['gaps'])):
            raise ValueError('automatic input intent scoped to one charge ability and mana siblings')
    if ui['disabled_tap_mode'] and (not p['cost']['tap'] or not any(line.startswith('Sacrifice this creature: ') for line in p['gaps'])):
        raise ValueError('disabled tap menu needs an independently legal self-sacrifice sibling')
    x_domain = ui.get('mana_sibling_x_domain')
    x_mana_sibling = ('{T}, Remove X charge counters from this artifact: '
                      'Add an amount of {C} equal to X plus one.')
    if x_domain is not None:
        if (not isinstance(x_domain, dict) or set(x_domain) != {'minimum', 'maximum', 'zero_charge_cost', 'zero_mana_result'}
                or any(type(x_domain[k]) is not int for k in ('minimum', 'zero_charge_cost', 'zero_mana_result'))
                or x_domain != dict(minimum=0, maximum='available_charge_counters', zero_charge_cost=0, zero_mana_result=1)
                or x_mana_sibling not in p['gaps'] or not ui['mana_shortage_query_only']):
            raise ValueError('zero-chosen-X mana sibling requires its complete printed clause and nonnegative available-counter domain')
    if ui['mana_shortage_query_only'] and not (any('Remove any number of charge counters' in line for line in p['gaps'])
                                             or x_domain is not None and x_mana_sibling in p['gaps']):
        raise ValueError('query-only shortage requires a zero-counter legal mana sibling')
    disabled_cost = ui.get('disabled_cost_modes', [])
    if disabled_cost:
        # Equal-total wrong-color mana still pays this independently printed,
        # untargeted token sibling. Keep the global query true and request the
        # actually disabled charge option using the existing disabled-menu path.
        sibling = re.compile(r'\{2\}, \{T\}, Remove a charge counter from this artifact: '
                             r'Create a [0-9]/[0-9] [A-Za-z ]+ creature token(?: with [a-z ,]+)?\.')
        if (disabled_cost != ['wrong_color'] or not cost_is_colored_two(p)
                or p['initial'] < 1 or not any(sibling.fullmatch(line) for line in p['gaps'])):
            raise ValueError('disabled cost control requires equal-total wrong-color mana and a legal untargeted charge-spending sibling')
    elif not isinstance(disabled_cost, list):
        raise ValueError('disabled cost controls require an explicit list')
    if ui['exact_cost_negatives'] and p['cost']['sacrifice'] != 'creature':
        raise ValueError('exact zero budget controls scoped to the creature-sacrifice profile')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(p))):
        raise ValueError('explicit distinct supported charge variants required')
    return p


def cost_is_colored_two(p):
    return p['cost']['tap'] and sum(p['activation_mana']) == 2 and any(p['activation_mana'][1:])


def variants(p):
    modes = ['charge', 'payment', 'offturn', 'tapped', 'bare_zones', 'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'cleanup']
    if p['cost']['sacrifice'] != 'self':
        modes += ['source_removed_pending', 'repeat']
    if p['cost']['tap'] and not p['cost']['sacrifice']:
        modes += ['repeat_untap']
    if p['creature']:
        modes += ['fresh']
    if sum(p['activation_mana']):
        modes += ['insufficient_mana']
    if any(p['activation_mana'][1:]):
        modes += ['wrong_color']
    if p['cost']['sacrifice'] == 'creature':
        modes += ['no_own_victim']
    if p['amount'] == 'X':
        modes += ['x0', 'x1', 'x5']
    if p['target']:
        modes += ['target_enemy', 'target_self', 'target_nonnative', 'target_removed_pending', 'probe_nonartifact']
    if 'counter' in p['cost']:
        modes += ['spend_all']
    return [mode + '_' + str(who) for who in (0, 1) for mode in modes]


def validate_witnesses(oracle, registry):
    for name, pin in pins()['witnesses'].items():
        if registry.get(name, {}).get('csvid') != pin['csvid'] or any(oracle.get(name, {}).get(k) != v for k, v in pin['oracle'].items()):
            raise ValueError('charge witness changed: ' + name)


def expand(name, csvid, declaration, p, selected, registry, oracle):
    from recipes import card, step
    validate_witnesses(oracle, registry)
    result = []; cost = p['cost']; ui = declaration['params']['interface']
    for variant in selected:
        mode, actor = variant.rsplit('_', 1); owner = int(actor); other = 1-owner
        x = {'x0':0, 'x1':1, 'x5':5}.get(mode, 3)
        amount = x if p['amount'] == 'X' else p['amount']
        activation = list(p['activation_mana']); activation[0] += x if cost['x'] else 0
        setup = []; entities = {}; hands = [[], []]; graves = [[], []]; libs = [[], []]; exiles = [[], []]
        totals = [80, 80]; actions = []; life = [20, 20]

        def add(nm, ref, who, zone='battlefield', state=None):
            if nm != name and nm not in pins()['witnesses']:
                raise ValueError('unpinned charge witness: ' + nm)
            item = card(nm, ref, who, zone)
            if zone == 'battlefield': item['state'] = list(state or ['untapped'])
            setup.append(item); record = oracle[nm]
            entities[ref] = dict(name=nm, player=who, alive=zone == 'battlefield', tapped='tapped' in (state or []), charge=0)
            if zone == 'hand': hands[who].append(nm)

        def leave(ref):
            e = entities[ref]; e['alive'] = False; graves[e['player']].append(e['name'])

        def receipts(stack=0):
            seq = [step('assert_stack', value=stack), step('assert_choices_consumed')]
            absent = set()
            for ref, e in entities.items():
                if not e['alive']:
                    absent.add((e['player'], e['name'])); continue
                seq += [step('require_card', ref=ref, property='in_play', value=1),
                        step('require_card', ref=ref, property='tapped', value=int(e['tapped'])),
                        step('assert_counter', ref=ref, counter_kind='charge', value=e['charge'])]
                if 'Creature' in oracle[e['name']]['type_line']:
                    seq.extend(step('require_card', ref=ref, property=prop, value=int(oracle[e['name']][prop])) for prop in ('power', 'toughness'))
            for who, nm in sorted(absent):
                seq.append(step('assert_zone_multiplicity', player=who, zone='battlefield', index=registry[nm]['csvid'],
                                value=sum(e['alive'] and e['player']==who and e['name']==nm for e in entities.values())))
            universe = {e['name'] for e in entities.values()} | {'Plains', 'Forest', 'Island'}
            for who in (0, 1):
                seq += [step('assert_life', player=who, value=20), step('assert_mana_total', player=who, value=totals[who]),
                        step('assert_zone', player=who, zone='battlefield', value=sum(e['alive'] and e['player']==who for e in entities.values()))]
                for zone, names in (('hand',hands[who]), ('graveyard',graves[who]), ('library',libs[who]), ('exile',exiles[who])):
                    seq.append(step('assert_zone', player=who, zone=zone, value=len(names)))
                    if zone == 'library':
                        seq.extend(step('assert_zone_card', player=who, zone=zone, index=i, value=registry[nm]['csvid']) for i,nm in enumerate(names))
                    else:
                        counts = Counter(names)
                        seq.extend(step('assert_zone_multiplicity', player=who, zone=zone, index=registry[nm]['csvid'], value=counts[nm]) for nm in sorted(universe))
            return seq

        if mode != 'bare_zones':
            for who in (0,1):
                for zone,nm in (('hand','Plains'), ('graveyard','Forest'), ('library','Plains'), ('exile','Island')):
                    setup.append(card(nm,f'{zone}_{who}',who,zone)); {'hand':hands,'graveyard':graves,'library':libs,'exile':exiles}[zone][who].append(nm)
        add(name, 'source', owner, 'battlefield' if p['land'] else 'hand')
        add('Lux Cannon', 'native', owner); add('Lux Cannon', 'enemy_native', other)
        add('Icy Manipulator', 'nonnative', owner)
        add('Forest', 'invalid', owner, state=['tapped'])
        if cost['sacrifice'] == 'creature':
            if mode != 'no_own_victim': add('Sage of Lat-Nam', 'victim', owner)
            add('Sage of Lat-Nam', 'enemy_victim', other)
        if mode == 'tapped': add('Icy Manipulator', 'tapper', other)
        untaps = 6 if mode == 'spend_all' else 1 if mode == 'repeat_untap' else 0
        for i in range(untaps): add('Twiddle', f'untap_{i}', owner, 'hand')
        target = ('enemy_native' if mode=='target_enemy' else 'source' if mode=='target_self'
                  else 'nonnative' if mode=='target_nonnative' else 'native') if p['target'] else 'source'
        actions += [step('turn',player=owner,value=0), step('present_ai_dialogs',value=1)]
        if not p['land']:
            actions.extend(step('set_mana',player=owner,color=c,value=n) for c,n in enumerate(p['casting_mana']))
            actions += [step('cast',ref='source',value=1), step('assert_stack',value=1), step('resolve'), step('refresh'),
                        step('assert_stack',value=0), step('assert_choices_consumed'), step('assert_mana_total',player=owner,value=0)]
            hands[owner].remove(name); entities['source']['alive']=True
        entities['source']['charge']=p['initial']
        actions.append(step('assert_counter',ref='source',counter_kind='charge',value=p['initial']))
        if p['creature'] and mode!='fresh':
            actions += [step('advance_turn'),step('advance_turn')]
        pools = [[30,10,10,10,10,10] for _ in (0,1)]
        if mode=='payment' or ui['exact_cost_negatives'] and mode in ('tapped','repeat','no_own_victim'):
            pools[owner]=list(activation)
        if mode=='insufficient_mana': pools[owner]=[0]*6
        if mode=='wrong_color':
            pools[owner]=list(activation); c=next(i for i in range(1,6) if pools[owner][i]); pools[owner][0]+=pools[owner][c]; pools[owner][c]=0
        totals=[sum(pool) for pool in pools]
        actions.extend(step('set_mana',player=who,color=c,value=n) for who in (0,1) for c,n in enumerate(pools[who]))
        if mode=='offturn': actions.append(step('turn',player=other,value=0))
        if mode=='tapped':
            actions += [step('choose_target',player=other,ref='source'),step('activate',ref='tapper',value=1),step('resolve'),step('refresh'),step('assert_choices_consumed')]
            totals[other]-=1; entities['tapper']['tapped']=True; entities['source']['tapped']=True
        actions.append(step('refresh')); ready=len(actions)
        legal = (mode not in ('insufficient_mana','wrong_color','no_own_victim')
                 and not (cost['tap'] and (mode=='tapped' or mode=='fresh' and p['creature']))
                 and not (p['sorcery'] and mode=='offturn'))

        def deny(disabled=False, query_only=False):
            seq=[step('require_can_activate',ref='source',value=int(disabled or query_only))]
            if disabled: seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            if not query_only: seq.append(step('activate',ref='source',value=0))
            return seq

        def activate():
            seq=[step('require_can_activate',ref='source',value=1)]
            if ui['intent']: seq.append(step('choose_activation_mode',ref='source',player=owner,**ui['intent']))
            elif str(owner) in ui['dialogs']: seq.append(step('choose_dialog',player=owner,value=ui['dialogs'][str(owner)]))
            if cost['x']: seq.append(step('choose_x',player=owner,value=x))
            if p['target']: seq.append(step('choose_target',player=owner,ref=target))
            if cost['sacrifice']=='creature' and ui['sacrifice_choices'].get(str(owner),False): seq.append(step('choose_target',player=owner,ref='victim'))
            seq.append(step('activate',ref='source',value=1))
            if ui['intent']: seq.append(step('require_activation_mode',value=1))
            totals[owner]-=sum(activation)
            if cost['tap']: entities['source']['tapped']=True
            if 'counter' in cost: entities['source']['charge']-=cost['counter']['amount']
            if cost['sacrifice']: leave('source' if cost['sacrifice']=='self' else 'victim')
            seq += receipts(1)
            return seq

        def resolve_add():
            if entities[target]['alive']: entities[target]['charge']+=amount
            return [step('resolve'),step('refresh'),step('assert_choices_consumed')]

        def untap(i):
            ref=f'untap_{i}'; hands[owner].remove('Twiddle'); graves[owner].append('Twiddle'); totals[owner]-=1
            entities['source']['tapped']=False
            return [step('choose_target',player=owner,ref='source'),step('cast',ref=ref,value=1),step('choose_dialog',player=owner,value=1),
                    step('resolve'),step('refresh'),step('assert_choices_consumed')]

        if mode=='probe_nonartifact':
            actions.append(step('probe_activation_target',ref='source',other='invalid',index=ui['dialogs'].get(str(owner),-1),value=0))
            legal=False
        elif mode in ('target_self','target_nonnative'):
            # A failing real target probe is an ordinary Oracle permission
            # assertion. Do not turn an engine target restriction into an
            # invalid queued-target harness failure or narrow Oracle scope.
            actions.append(step('probe_activation_target',ref='source',other=target,index=ui['dialogs'].get(str(owner),-1),value=1))
        pending=None
        if mode!='probe_nonartifact':
            if not legal:
                actions += deny((ui['disabled_tap_mode'] and mode in ('tapped','fresh')) or mode in ui.get('disabled_cost_modes', []),
                                ui['mana_shortage_query_only'] and mode=='insufficient_mana')
            else:
                actions += activate(); pending=len(actions)
                if mode=='source_removed_pending': actions += [step('destroy',ref='source'),step('refresh')]; leave('source')
                if mode=='target_removed_pending': actions += [step('destroy',ref=target),step('refresh')]; leave(target)
                actions += resolve_add()
                if mode=='repeat':
                    if cost['tap']: actions += deny(ui['disabled_tap_mode'])
                    else: actions += activate()+resolve_add()
                if mode=='repeat_untap': actions += untap(0)+activate()+resolve_add()
                if mode=='spend_all':
                    for i in range(5): actions += untap(i)+activate()+resolve_add()
                    actions += untap(5)+deny()
                if mode=='cleanup': actions += [step('cleanup'),step('refresh')]
        checks=receipts(); actions += checks
        if mode=='payment' and legal: actions.extend(step('assert_mana',player=owner,color=c,value=0) for c in range(6))
        if mode in ('ready_snapshot','pending_snapshot') and legal:
            cut=ready if mode=='ready_snapshot' else pending; tail=actions[cut:]
            actions=actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if mode=='resolved_snapshot' and legal:
            actions += [step('capture'),step('record_state'),step('set_life',player=other,value=7),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'ChargeCounterActivationRecipe_{csvid}_{p["ability"]}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=actions,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                                        untested_components=p['gaps']+['first-main conversion event is not synthesized','other activated sibling effects','replacement/proliferation effects',
                                        'unaffordable selected charge-mode attempt when a mana-only fallback autoselects'])))
    return result
