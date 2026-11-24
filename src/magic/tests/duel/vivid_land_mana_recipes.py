"""Actual Vivid land entry and counter-funded mana abilities."""
from collections import Counter
import copy
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'vivid_land_mana'
PROFILE = Path(__file__).parent / 'data/vivid_land_mana_wave8_profiles.json'
ENTRY = 'This land enters tapped with two charge counters on it.'
CHARGE_COST = '{T}, Remove a charge counter from this land: Add one mana of any color.'
COLORS = {'B':1,'U':2,'G':3,'R':4,'W':5}
COMMON = ('entry_tapped','fixed_charged','deplete','free_interleaved','repeat_denied','offturn_alternate',
          'pending_entry_replay','one_charge_replay','reentry_reset','spend_blue')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text','').splitlines()
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line')!='Land' or record.get('mana_cost')!='' or record.get('keywords')!=[] or
            len(lines)!=3 or lines[0]!=ENTRY or lines[2]!=CHARGE_COST):
        raise ValueError('whole normal land, tapped charge entry and alternate charge cost required')
    fixed = re.fullmatch(r'\{T\}: Add \{([BUGRW])\}\.',lines[1])
    if not fixed:
        raise ValueError('exact printed free mana ability required')
    return dict(schema=1,fixed_color=fixed[1],entry_tapped=True,entry_charge=2,alternate_cost=1)


def variants(model):
    modes = COMMON + tuple('alternate_'+c.lower() for c in COLORS if c!=model['fixed_color'])
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def spec(declaration, record):
    model = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(declaration.get('params'),model)):
        raise ValueError('typed whole Vivid land declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('explicit distinct valid Vivid land variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, registry):
    for group, records in pins().items():
        if group not in ('sources','witnesses'):
            continue
        for name, pin in records.items():
            record, row = oracle.get(name,{}), registry.get(name,{})
            if (not exact(record,pin['oracle']) or not exact(row,pin['registry']) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Vivid {group} identity changed: {name}')


def scenario(name, csvid, model, registry, oracle, variant):
    from recipes import card, cost_from_oracle, step

    mode, owner = variant.rsplit('_',1)
    owner = int(owner)
    enemy = 1-owner
    fixed = model['fixed_color']
    alternate, other_alt = [color for color in COLORS if color!=fixed][:2]
    setup, steps, objects = [], [], {}
    wallet = [[0]*6 for _ in (0,1)]
    charges = 0
    snapshot = None

    def emit(op, **values):
        steps.append(step(op,**values))

    def add(ref, printing, player, zone):
        setup.append(card(printing,ref,player,zone))
        objects[ref] = dict(name=printing,player=player,zone=zone,tapped=False)

    def prop(ref, key, value, required=False):
        emit('require_card' if required else 'assert_card',ref=ref,property=key,value=int(value))

    def pools():
        for player, amounts in enumerate(wallet):
            for color, amount in enumerate(amounts):
                emit('assert_mana',player=player,color=color,value=amount)

    def status():
        emit('assert_stack',value=0)
        emit('assert_choices_consumed')
        emit('assert_counter',ref='source',counter_kind='charge',value=charges)
        prop('source','tapped',objects['source']['tapped'])
        pools()

    def fund_blue(player, amount):
        if any(wallet[player]):
            raise ValueError('fixture blue funding requires an empty pool')
        wallet[player][COLORS['U']] = amount
        emit('set_mana',player=player,color=COLORS['U'],value=amount)
        emit('refresh')

    def resolve():
        emit('resolve')
        emit('refresh')

    def enter(capture=False):
        nonlocal snapshot, charges
        emit('require_can_activate',ref='atlas',value=1)
        emit('activate',ref='atlas',value=1)
        emit('assert_choices_consumed')
        emit('assert_stack',value=1)
        prop('atlas','tapped',1)
        prop('source','in_hand',1)
        if capture:
            snapshot = len(steps)
        emit('choose_zone_card',player=owner,value=csvid)
        resolve()
        charges = model['entry_charge']
        objects['atlas']['tapped'] = True
        objects['source'].update(zone='battlefield',tapped=model['entry_tapped'])
        prop('source','in_play',1,required=True)
        status()

    def deny_tapped():
        prop('source','tapped',1,required=True)
        emit('require_can_activate',ref='source',value=0)
        emit('activate',ref='source',value=0)
        status()

    def advance(player):
        if any(map(any,wallet)):
            raise ValueError('real untap starts with an empty pool')
        emit('advance_turn')
        emit('untap_step',player=player)
        emit('upkeep_step',player=player)
        emit('turn',player=player,value=0)

    def ready():
        advance(enemy)
        prop('source','tapped',1)
        advance(owner)
        for perm in ('atlas','source'):
            objects[perm]['tapped'] = False
            prop(perm,'tapped',0,required=True)
        status()

    def mana(color):
        nonlocal charges
        if color!=fixed and charges<model['alternate_cost']:
            raise ValueError('alternate mana needs a real charge counter')
        emit('require_can_activate',ref='source',value=1)
        # The production selector merges the two printed abilities. A charged
        # choice of printed color follows the free route. The depleted palette
        # is a singleton and needs no color input.
        if charges:
            emit('choose_color',player=owner,value=COLORS[color])
        emit('activate',ref='source',value=1)
        objects['source']['tapped'] = True
        wallet[owner][COLORS[color]] += 1
        if color!=fixed:
            charges -= model['alternate_cost']
        status()

    def spell(ref, untap):
        entity = objects[ref]
        player = entity['player']
        cost = cost_from_oracle(oracle[entity['name']])
        if any(amount<fee for amount,fee in zip(wallet[player],cost)):
            raise ValueError('actual wallet must pay the declared helper spell')
        emit('choose_target',player=player,ref='source')
        emit('cast',ref=ref,value=1)
        emit('assert_stack',value=1)
        if untap:
            emit('choose_dialog',player=player,value=1)
        resolve()
        entity['zone'] = 'graveyard'
        wallet[player] = [amount-fee for amount,fee in zip(wallet[player],cost)]
        if untap:
            objects['source']['tapped'] = False
            status()
        else:
            emit('bind_hand',ref='source',player=owner,value=csvid)
            prop('source','in_hand',1,required=True)
            objects['source'].update(zone='hand',tapped=False)

    add('source',name,owner,'hand')
    add('atlas','Walking Atlas',owner,'battlefield')
    repeat_count = {'deplete':2,'free_interleaved':4,'one_charge_replay':1}.get(mode,0)
    for index in range(repeat_count):
        add(f'twiddle_{index}','Twiddle',enemy,'hand')
    if mode=='spend_blue':
        add('blue_payment','Twiddle',owner,'hand')
    if mode=='reentry_reset':
        add('bounce','Boomerang',enemy,'hand')
    emit('turn',player=owner,value=0)
    emit('present_ai_dialogs',value=1)
    for player in (0,1):
        for color in range(6):
            emit('set_mana',player=player,color=color,value=0)
    emit('refresh')
    enter(capture=mode=='pending_entry_replay')
    if mode=='entry_tapped':
        deny_tapped()
    else:
        ready()
        if repeat_count:
            fund_blue(enemy,repeat_count)
        if mode=='offturn_alternate':
            advance(enemy)
        if mode.startswith('alternate_'):
            mana(mode[-1].upper())
        elif mode=='fixed_charged':
            mana(fixed)
        elif mode=='deplete' or mode=='free_interleaved':
            sequence = [alternate,other_alt,fixed] if mode=='deplete' else [fixed,alternate,fixed,other_alt,fixed]
            for index, color in enumerate(sequence):
                if index:
                    spell(f'twiddle_{index-1}',untap=True)
                mana(color)
        elif mode=='spend_blue':
            mana('U')
            spell('blue_payment',untap=True)
        else:
            mana(alternate)
            if mode=='repeat_denied':
                deny_tapped()
            if mode=='one_charge_replay':
                snapshot = len(steps)
                spell('twiddle_0',untap=True)
                mana(other_alt)
            if mode=='reentry_reset':
                fund_blue(enemy,2)
                spell('bounce',untap=False)
                enter()
                deny_tapped()
    status()
    prop('atlas','tapped',objects['atlas']['tapped'])
    for player in (0,1):
        emit('assert_life',player=player,value=20)
        for zone in ('hand','battlefield','library','graveyard','exile'):
            counts = Counter(obj['name'] for obj in objects.values() if obj['player']==player and obj['zone']==zone)
            emit('assert_zone',player=player,zone=zone,value=sum(counts.values()))
            for printing, amount in counts.items():
                emit('assert_zone_multiplicity',player=player,zone=zone,index=registry[printing]['csvid'],value=amount)
    if snapshot is not None:
        tail = steps[snapshot:]
        steps = (steps[:snapshot]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                 +copy.deepcopy(tail)+[step('assert_recorded_state')])
    return setup, steps


def expand(name, csvid, declaration, model, selected, registry, oracle):
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved Vivid land source')
    cases = []
    for variant in selected:
        setup, steps = scenario(name,csvid,model,registry,oracle,variant)
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'Distinct paid selection of the printed color through the merged selector',
                            'Manual land-play permissions and external mana replacements'])
        cases.append(dict(name=f'VividLandManaRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=setup,steps=steps,_recipe=metadata))
    return cases
