"""Fallen Empires storage lands through real land plays, turns and mana activations."""
import copy
import json
import re
from pathlib import Path

from guild_locket_recipes import SPENDERS
from library_search_recipes import wave6_exact as exact_pin

FAMILY = 'storage_counter_land'
DATA = Path(__file__).parent / 'data/storage_counter_land_wave8_profiles.json'
COLORS = 'CBUGRW'
CLAUSES = ('This land enters tapped.',
           'You may choose not to untap this land during your untap step.',
           'At the beginning of your upkeep, if this land is tapped, put a storage counter on it.')
OUTPUT = re.compile(r'\{T\}, Remove any number of storage counters from this land: Add \{([BUGRW])\} for each storage counter removed this way\.')
MODES = ('entry', 'offturn_play', 'land_quota', 'opponent_upkeep', 'own_upkeep', 'bank_three',
         'untap_banked', 'cash_partial', 'cash_all', 'cash_zero', 'cash_cancel', 'spend_mana',
         'offturn_mana', 'replay_upkeep', 'replay_untap', 'replay_cash', 'recharge')
PAYMENTS = {'cash_partial': 1, 'cash_all': 3, 'cash_zero': 0, 'cash_cancel': -1,
            'spend_mana': 1, 'offturn_mana': 2, 'replay_cash': 2, 'recharge': 1}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or record.get('type_line') != 'Land' or record.get('mana_cost') != '' or record.get('keywords') != []):
        raise ValueError('whole normal nonbasic land required')
    clauses = record['oracle_text'].splitlines()
    if len(clauses) != 4 or tuple(clauses[:3]) != CLAUSES or not (match := OUTPUT.fullmatch(clauses[3])):
        raise ValueError('all four exact Fallen Empires storage land clauses required')
    return dict(color=match[1], clauses=clauses)


def spec(declaration, record):
    model = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int
            or params['schema'] != 1 or not exact_pin(params['mechanism'], model)):
        raise ValueError('whole independent source and storage ability pins required')
    return dict(model, csvid=source['csvid'], family=FAMILY)


def variants(model):
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES] + ['explicit_early_0', 'zero_empty_0']


def validate_witnesses(oracle, rows):
    from recipes import cost_from_oracle

    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != {'Forest', 'Mountain', *SPENDERS.values()}:
        raise ValueError('exact storage land witness pool required')
    for group in ('sources', 'witnesses'):
        for name, entry in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(entry['oracle'], oracle.get(name)) or row.get('kind') != 'card'
                    or row.get('csvid') != entry['csvid'] or row.get('handler') != entry['handler']
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('implemented independent storage source/witness changed: ' + name)
    for color, name in SPENDERS.items():
        cost = cost_from_oracle(oracle[name])
        if sum(cost) != 1 or cost[COLORS.index(color)] != 1 or not oracle[name]['type_line'].startswith('Creature — '):
            raise ValueError('one actual colored mana spender required: ' + name)


def _case(name, csvid, declaration, model, variant, rows):
    from recipes import card, step

    mode, actor = variant.rsplit('_', 1)
    owner = int(actor)
    player = 1 - owner if mode == 'offturn_play' else owner
    color = COLORS.index(model['color'])
    spender = SPENDERS[model['color']]
    setup = [card(name, 'source', owner, 'hand')]
    if mode == 'land_quota':
        setup.append(card('Forest', 'second', owner, 'hand'))
    if mode in ('spend_mana', 'recharge'):
        setup.append(card(spender, 'spender', owner, 'hand'))
    for side in (0, 1):
        setup.extend(card(name, f'library_{side}_{index}', side, 'library')
                     for index, name in enumerate(('Forest', 'Mountain')))
    steps = []
    zones = {entry['ref']:entry['zone'] for entry in setup}
    tapped = False
    counters = 0
    mana = 0
    replay_from = None

    def emit(op, **values):
        steps.append(step(op, **values))

    def receipts():
        emit('assert_choices_consumed')
        emit('assert_stack', value=0)
        for side in (0,1):
            emit('assert_life', player=side, value=20)
            emit('assert_mana_total', player=side, value=mana if side==owner else 0)
            emit('assert_mana', player=side, color=color, value=mana if side==owner else 0)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                emit('assert_zone', player=side, zone=zone,
                     value=sum(zones[e['ref']]==zone for e in setup if e['player']==side))
            for index, library_name in enumerate(('Forest','Mountain')):
                emit('assert_zone_card', player=side, zone='library', index=index, value=rows[library_name]['csvid'])
        for item in setup:
            ref = item['ref']
            if not ref.startswith('library_'):
                emit('assert_zone_multiplicity', player=item['player'], zone=zones[ref], index=rows[item['name']]['csvid'], value=1)
        if zones['source']=='battlefield':
            emit('assert_card', ref='source', property='tapped', value=int(tapped))
            emit('assert_counter', ref='source', counter_kind='storage', value=counters)

    def play(ref='source', allowed=True):
        nonlocal tapped
        emit('play_land', ref=ref, value=int(allowed))
        if allowed:
            zones[ref] = 'battlefield'
            if ref=='source':
                tapped = True
        receipts()

    def visit(untap=False, replay=None):
        nonlocal player, tapped, counters, replay_from
        if mana:
            raise ValueError('real storage upkeep must start with empty pools')
        player = 1-player
        emit('advance_turn')
        own_tapped = player==owner and zones['source']=='battlefield' and tapped
        if own_tapped and owner==1 and untap!=(counters>2):
            raise ValueError('the native AI untaps its storage land only above two counters')
        if replay=='untap':
            replay_from = len(steps)
        if own_tapped and owner==0:
            if untap:
                emit('choose_priority', player=owner, ref='source')
            else:
                emit('choose_priority', player=owner, value=-2)
        emit('untap_step', player=player)
        if own_tapped and untap:
            tapped = False
        receipts()
        if replay=='upkeep':
            replay_from = len(steps)
        emit('upkeep_step', player=player)
        if player==owner and zones['source']=='battlefield' and tapped:
            counters += 1
        receipts()
        emit('turn', player=player, value=0)

    def own_visit(untap=False, replay=None):
        if player==owner:
            visit()
        visit(untap,replay)

    def cash(amount):
        nonlocal mana, counters, tapped
        if tapped or not -1 <= amount <= counters:
            raise ValueError('storage mana selection needs a genuinely available amount')
        emit('can_activate', ref='source', value=1)
        emit('choose_number', player=owner, value=amount)
        emit('activate', ref='source', value=int(amount>=0))
        if amount>=0:
            mana += amount
            counters -= amount
            tapped = True
        receipts()

    def spend():
        nonlocal mana
        if mana != 1:
            raise ValueError('the actual colored spell must spend exactly one banked mana')
        emit('cast', ref='spender', value=1)
        emit('assert_stack', value=1)
        emit('resolve')
        mana -= 1
        zones['spender'] = 'battlefield'
        receipts()

    emit('turn', player=player, value=0)
    for side in (0,1):
        for c in range(6):
            emit('set_mana', player=side, color=c, value=0)
    emit('refresh')
    if mode=='offturn_play':
        play(allowed=False)
        own_visit()
    play()
    if mode=='entry':
        emit('can_activate', ref='source', value=0)
        emit('activate', ref='source', value=0)
    if mode=='land_quota':
        play('second', allowed=False)
        own_visit()
        play('second')
    if mode=='opponent_upkeep':
        visit()
    if mode in ('own_upkeep','replay_upkeep'):
        own_visit(replay='upkeep' if mode=='replay_upkeep' else None)
    if mode in ('explicit_early','zero_empty'):
        own_visit(untap=True)
        if mode=='zero_empty':
            cash(0)
    if mode in {'bank_three','untap_banked','replay_untap',*PAYMENTS}:
        for _ in range(3):
            own_visit()
        if mode!='bank_three':
            own_visit(untap=True,replay='untap' if mode=='replay_untap' else None)
        if mode=='offturn_mana':
            visit()
        if mode in PAYMENTS:
            if mode=='replay_cash':
                replay_from=len(steps)
            cash(PAYMENTS[mode])
        if mode in ('spend_mana','recharge'):
            spend()
        if mode=='recharge':
            own_visit()
    if replay_from is not None:
        suffix = steps[replay_from:]
        steps = (steps[:replay_from]+[step('capture')]+suffix+[step('record_state'),step('restore'),step('ai_roundtrip')]
                 +copy.deepcopy(suffix)+[step('assert_recorded_state')])
    return dict(name=f'StorageCounterLandRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=setup,steps=steps,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=model['clauses'],untested_components=[
                            'The native AI chooses its untap policy and does not offer manual untap or zero-at-empty choices.',
                            'Multiple simultaneous optional untaps, mana-source auto-selection, and counter/land-type replacement effects.']))


def expand(name, csvid, declaration, model, selected, rows, oracle):
    if csvid!=model['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(model)):
        raise ValueError('explicit reviewed storage land variants required')
    validate_witnesses(oracle,rows)
    return [_case(name,csvid,declaration,model,variant,rows) for variant in selected]
