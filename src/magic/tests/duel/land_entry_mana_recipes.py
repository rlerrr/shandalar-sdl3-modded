"""Land entry and mana receipts through Walking Atlas, never a land spell cast."""
import copy
import re
from collections import Counter
from grant_recipes import HAND, GRAVE, EXILE, LIBRARY
from mana_creature_recipes import output_pool, WITNESSES, HULK

ATLAS = ('{2}', '{T}: You may put a land card from your hand onto the battlefield.', '1', '1')
SPELLS = {
    'Twiddle': ('{U}', 'You may tap or untap target artifact, creature, or land.'),
    'Boomerang': ('{U}{U}', "Return target permanent to its owner's hand."),
    'Suppression Field': ('{1}{W}', "Activated abilities cost {2} more to activate unless they're mana abilities."),
}
SCRY_REMINDERS = ('(Look at the top card of your library, then you may put that card on the bottom.)',
                  '(Look at the top card of your library. You may put that card on the bottom.)')


def spec(d, r):
    if not r or r.get('layout') != 'normal' or r.get('card_faces') or r.get('selected_face') or 'Land' not in r.get('type_line', '').split(' — ')[0].split() or 'Creature' in r.get('type_line', '') or d.get('oracle_text') != r.get('oracle_text') or 'variants' not in d:
        raise ValueError('whole normal land Oracle pin and explicit entry variants required')
    p = d.get('params')
    if not isinstance(p, dict) or set(p) != {'entry', 'outputs', 'production', 'life'}:
        raise ValueError('explicit entry form, ordinary mana outputs and life amount required')
    if p['entry'] not in ('shock', 'bounce', 'scry', 'scry_untapped', 'multiplayer', 'life', 'life_untapped', 'tapped', 'lair') or type(p['life']) is not int:
        raise ValueError('reviewed entry form and integer life amount required')
    outputs = p['outputs']
    if not isinstance(outputs, list) or not outputs or any(not isinstance(o, str) for o in outputs) or len(set(outputs)) != len(outputs):
        raise ValueError('unique mana outputs required')
    pools = [output_pool(o) for o in outputs]
    if p['production'] == 'fixed' and len(outputs) == 1:
        wording = outputs[0]
    elif p['production'] == 'choice' and 2 <= len(outputs) <= 3 and all(sum(pool) == 1 for pool in pools):
        wording = ' or '.join(outputs) if len(outputs) == 2 else ', '.join(outputs[:-1]) + ', or ' + outputs[-1]
    else:
        raise ValueError('one fixed output or two/three ordinary color choices required')
    mana = '{T}: Add ' + wording + '.'
    lines = r['oracle_text'].splitlines(); remaining = []
    for line in list(lines):
        if re.fullmatch(r'Cycling ((?:\{[1-9BUGRW]\})+)(?: \([^\n]+\))?', line):
            remaining.append(line); lines.remove(line)
    mode = p['entry']
    if mode == 'shock':
        if p['life'] not in (2, 3) or not r['type_line'].startswith('Land — '):
            raise ValueError('reviewed typed land and explicit two/three-life option required')
        expected = ['(' + mana + ')', f"As this land enters, you may pay {p['life']} life. If you don't, it enters tapped."]
    elif mode == 'bounce':
        expected = ['This land enters tapped.', "When this land enters, return a land you control to its owner's hand.", mana]
    elif mode == 'lair':
        if r['type_line'] != 'Land — Lair':
            raise ValueError('printed Lair subtype required')
        expected = ["When this land enters, sacrifice it unless you return a non-Lair land you control to its owner's hand.", mana]
    elif mode.startswith('scry'):
        expected = (['This land enters tapped.'] if mode == 'scry' else []) + ['When this land enters, scry 1.', mana]
        for reminder in SCRY_REMINDERS:
            lines = [line.replace(' ' + reminder, '') for line in lines]
    elif mode == 'multiplayer':
        expected = ['This land enters tapped unless you have two or more opponents.', mana]
    elif mode.startswith('life'):
        if not 1 <= p['life'] <= 3:
            raise ValueError('one to three entry life gain required')
        expected = (['This land enters tapped.'] if mode == 'life' else []) + [f"When this land enters, you gain {p['life']} life.", mana]
    else:
        expected = ['This land enters tapped.', mana]
    if mode not in ('shock', 'life', 'life_untapped') and p['life']:
        raise ValueError('life amount requires a printed life clause')
    if Counter(lines) != Counter(expected) or set(r.get('keywords', [])) != ({'Cycling'} if remaining else set()) | ({'Scry'} if mode.startswith('scry') else set()):
        raise ValueError('parameters must match every Oracle entry and mana clause')
    return dict(p, family='land_entry_mana', pools=pools, untested_rules=remaining, legendary='Legendary' in r['type_line'])


def variants(p):
    common = ['entry', 'opposing_turn', 'cleanup', 'bare_zones', 'ready_snapshot', 'pending_snapshot',
              'resolved_snapshot', 'two_sources', 'reenter', 'other_land']
    if p['entry'] == 'shock': common += ['decline_life', 'insufficient_life']
    if p['entry'] in ('bounce', 'lair'): common += ['no_other_lands', 'return_alternate', 'return_untapped']
    if p['entry'] == 'bounce': common += ['return_self']
    if p['entry'] == 'lair': common += ['decline_return']
    if p['entry'].startswith('scry'): common += ['bottom', 'short_library', 'empty_library']
    if p['legendary']: common.remove('two_sources')
    result = [v + '_' + str(owner) for owner in (0, 1) for v in common]
    for owner in (0, 1):
        for i, pool in enumerate(p['pools']):
            modes = ['production', 'repeat_denied', 'untap_repeat', 'tax', 'mana_snapshot', 'spend_generic']
            if any(pool[1:]): modes.append('spend_colored')
            result += [f'{v}_{i}_{owner}' for v in modes]
    return result


def validate_witnesses(oracle, registry):
    r = oracle.get('Walking Atlas', {})
    if 'Walking Atlas' not in registry or tuple(r.get(k) for k in ('mana_cost', 'oracle_text', 'power', 'toughness')) != ATLAS:
        raise ValueError('land entry witness changed: Walking Atlas')
    for nm, fields in SPELLS.items():
        if nm not in registry or (oracle.get(nm, {}).get('mana_cost'), oracle.get(nm, {}).get('oracle_text')) != fields:
            raise ValueError('land entry spell witness changed: ' + nm)
    for nm in ('Forest', 'Island', 'Mountain'):
        if nm not in registry or oracle.get(nm, {}).get('type_line') != 'Basic Land — ' + nm:
            raise ValueError('land entry basic land witness changed: ' + nm)
    for nm, mana, power, toughness, text in [*WITNESSES.values(), HULK]:
        r = oracle.get(nm, {})
        if nm not in registry or tuple(r.get(k) for k in ('mana_cost', 'power', 'toughness', 'oracle_text')) != (mana, str(power), str(toughness), text):
            raise ValueError('land entry spending witness changed: ' + nm)


def expand(name, csvid, d, p, selected, registry, oracle):
    from recipes import card, step, observe, cost_from_oracle
    result = []
    for variant in selected:
        parts = variant.rsplit('_', 2); indexed = len(parts) == 3 and parts[-2].isdigit()
        if indexed: v, index, owner = parts[0], int(parts[1]), int(parts[2])
        else: v, owner = variant.rsplit('_', 1); owner = int(owner); index = 0
        kind = p['entry']; bare = v == 'bare_zones'
        zones = {z: [[] if bare else list(ns), [] if bare else list(ns)] for z, ns in (('hand', HAND), ('graveyard', GRAVE), ('exile', EXILE), ('library', LIBRARY))}
        if bare: zones['library'] = [list(LIBRARY), list(LIBRARY)]
        if v == 'empty_library': zones['library'][owner] = []
        if v == 'short_library': zones['library'][owner] = ['Forest']
        life = [20, 20]; mana = [[20]*6, [20]*6]; setup = []; entities = {}; actions = []
        pre = [step('turn', player=1-owner if v == 'opposing_turn' else owner, value=0)]
        if v == 'insufficient_life': life[owner] = p['life']-1; pre.append(step('set_life', player=owner, value=life[owner]))
        for pl in (0, 1):
            pre += [step('set_mana', player=pl, color=c, value=n) for c, n in enumerate(mana[pl])]

        def add(nm, ref, pl, zone='battlefield', tapped=False):
            item = card(nm, ref, pl, zone)
            if tapped: item['state'] = ['tapped']
            setup.append(item); entities[ref] = dict(name=nm, player=pl, alive=zone == 'battlefield', hand=zone == 'hand', tapped=tapped)
            if zone != 'battlefield': zones[zone][pl].append(nm)

        def spend(nm, pl): mana[pl] = [a-b for a, b in zip(mana[pl], cost_from_oracle(oracle[nm]))]

        def move(ref, zone):
            e = entities[ref]; e['alive'] = False; e['hand'] = zone == 'hand'; zones[zone][e['player']].append(e['name'])

        def spell(nm, ref, target=None, dialog=None):
            caster = owner if nm == 'Suppression Field' else 0
            add(nm, ref, caster, 'hand'); spend(nm, caster); zones['hand'][caster].remove(nm); entities[ref]['hand'] = False
            if any(t in oracle[nm]['type_line'] for t in ('Instant','Sorcery')): zones['graveyard'][caster].append(nm)
            else: entities[ref]['alive'] = True
            seq = ([step('choose_target', player=caster, ref=target)] if target else []) + [step('cast', ref=ref, value=1)]
            if dialog is not None: seq.append(step('choose_dialog', player=caster, value=dialog))
            return seq + [step('resolve'), step('assert_choices_consumed'), step('refresh')]

        def untap(ref, label):
            seq = spell('Twiddle', label, ref, 1); entities[ref]['tapped'] = False
            return seq + [observe(ref, 'tapped', 0)]

        def receipt(ref):
            e = entities[ref]; seq = [step('require_card', ref=ref, property='in_play', value=int(e['alive']))]
            if e['alive']: seq += [step('require_card', ref=ref, property='csvid', value=csvid), observe(ref, 'tapped', int(e['tapped']))]
            if e['hand']: seq += [step('bind_hand', player=owner, value=csvid, ref=ref)]
            seq += [observe(ref, 'in_hand', int(e['hand'])), step('assert_life', player=owner, value=life[owner])]
            return seq

        add('Walking Atlas', 'atlas', owner)
        if v != 'no_other_lands':
            add('Forest', 'own_land', owner, tapped=v != 'return_untapped'); add('Island', 'own_other', owner, tapped=True)
        add('Mountain', 'enemy_land', 1-owner, tapped=True)
        add(name, 'source', owner, 'hand')
        if v == 'two_sources': add(name, 'second', owner, 'hand')
        if v == 'other_land': add('Mountain', 'later', owner, 'hand')

        def enter(ref='source', return_ref='own_land'):
            source_name = entities[ref]['name']; source_kind = kind if source_name == name else None
            returned = None
            seq = ([step('record_library', player=owner)] if source_kind and source_kind.startswith('scry') else [])
            seq += [step('require_can_activate', ref='atlas', value=1), step('activate', ref='atlas', value=1), step('assert_stack', value=1), observe(ref, 'in_hand', 1), step('choose_zone_card', player=owner, value=registry[source_name]['csvid'])]
            entities['atlas']['tapped'] = True
            zones['hand'][owner].remove(source_name); e = entities[ref]; e['alive'] = True; e['hand'] = False
            e['tapped'] = source_kind not in (None, 'lair', 'shock', 'life_untapped', 'scry_untapped')
            if source_kind == 'shock':
                paying = v not in ('decline_life', 'insufficient_life')
                seq.append(step('choose_dialog', player=owner, value=int(paying)))
                if paying: life[owner] -= p['life']
                e['tapped'] = not paying
            if source_kind and source_kind.startswith('life'): life[owner] += p['life']
            if source_kind in ('bounce', 'lair'):
                decline = source_kind == 'lair' and v in ('no_other_lands', 'decline_return')
                if decline:
                    seq.append(step('choose_cancel_target', player=owner))
                    move(ref, 'graveyard')
                else:
                    dest = ref if v in ('return_self', 'no_other_lands') else 'own_other' if v == 'return_alternate' else return_ref
                    seq.append(step('choose_target', player=owner, ref=dest)); move(dest, 'hand')
                    returned = dest
            if source_kind and source_kind.startswith('scry'):
                bottom = v in ('bottom', 'two_sources', 'reenter', 'mana_snapshot')
                if owner == 0 and zones['library'][owner]:
                    seq.append(step('choose_zone', player=owner, value=0 if bottom else -1))
                    if bottom: zones['library'][owner].append(zones['library'][owner].pop(0))
                elif zones['library'][owner]:
                    # AI scry may invoke the tutor selector or skip it according
                    # to board policy. Explicitly retain when offered, allowing
                    # that optional selector to be absent as well.
                    seq.append(step('choose_optional_zone', player=owner, value=registry[zones['library'][owner][0]]['csvid']))
            seq += [step('resolve'), step('assert_choices_consumed'), step('refresh')]
            if returned is not None:
                seq.append(step('bind_hand', player=owner, value=registry[entities[returned]['name']]['csvid'], ref=returned))
            if source_name == name: seq += receipt(ref)
            else: seq += [observe(ref, 'in_play', 1), observe(ref, 'tapped', 0)]
            if source_kind and source_kind.startswith('scry'):
                seq += [step('assert_scry_result', player=owner, value=1),step('assert_forced_zone_choices',value=0)]
            return seq

        actions += enter(return_ref='own_land')
        pending_cut = next(i for i, s in enumerate(actions) if s['op'] == 'resolve')
        if v in ('two_sources', 'reenter', 'other_land'):
            if v == 'reenter':
                actions += spell('Boomerang', 'return_source', 'source'); move('source', 'hand')
                actions += [step('bind_hand', player=owner, value=csvid, ref='source')]
            actions += untap('atlas', 'repeat_atlas')
            actions += enter('second' if v == 'two_sources' else 'later' if v == 'other_land' else 'source', 'own_other' if kind in ('bounce', 'lair') else 'own_land')
        mana_cut = 0
        if indexed:
            if entities['source']['tapped']: actions += untap('source', 'entry_untap')
            if v == 'tax': actions += spell('Suppression Field', 'taxer')
            if v in ('spend_generic', 'spend_colored'):
                mana[owner] = [6-sum(p['pools'][index]), 0, 0, 0, 0, 0] if v == 'spend_generic' else [0]*6
                actions += [step('set_mana', player=owner, color=c, value=n) for c, n in enumerate(mana[owner])] + [step('refresh')]
            def activate():
                seq = [step('require_can_activate', ref='source', value=1)]
                if p['production'] == 'choice': seq.append(step('choose_color', player=owner, value=next(c for c,n in enumerate(p['pools'][index]) if n)))
                seq += [step('activate', ref='source', value=1), step('assert_choices_consumed'), step('assert_stack', value=0)]
                entities['source']['tapped'] = True; mana[owner] = [a+b for a,b in zip(mana[owner],p['pools'][index])]
                return seq + [observe('source', 'tapped', 1)] + [step('assert_mana', player=owner, color=c, value=n) for c,n in enumerate(mana[owner])]
            actions += activate(); mana_cut = len(actions)
            if v == 'repeat_denied': actions += [step('require_can_activate', ref='source', value=0), step('activate', ref='source', value=0)]
            if v == 'untap_repeat': actions += untap('source', 'repeat_source') + activate()
            if v in ('spend_generic', 'spend_colored'):
                names = [HULK[0]] if v == 'spend_generic' else [WITNESSES['{'+'CBUGRW'[c]+'}'][0] for c,n in enumerate(p['pools'][index]) if c for _ in range(n)]
                for i, nm in enumerate(names):
                    ref = 'spender_'+str(i); add(nm,ref,owner,'hand')
                    if v == 'spend_generic' and owner == 0: actions += [step('choose_mana',player=owner,color=c) for c,n in enumerate(p['pools'][index]) if c for _ in range(n)]
                    actions += [step('cast',ref=ref,value=1),step('resolve'),step('assert_choices_consumed')]
                    spend(nm,owner); zones['hand'][owner].remove(nm); entities[ref]['alive']=True;entities[ref]['hand']=False
                mana[owner] = [0]*6
        if v == 'cleanup': actions += [step('cleanup')]
        checks = [step('assert_stack', value=0), step('assert_choices_consumed')]
        for ref,e in entities.items():
            checks += [observe(ref,'in_play',int(e['alive'])),observe(ref,'in_hand',int(e['hand']))]
            if e['alive']:
                checks.append(observe(ref,'tapped',int(e['tapped'])))
                if 'Creature' in oracle[e['name']]['type_line']: checks += [observe(ref,'power',int(oracle[e['name']]['power'])),observe(ref,'toughness',int(oracle[e['name']]['toughness'])),observe(ref,'damage',0)]
        universe = set(HAND+GRAVE+EXILE+LIBRARY)|{e['name'] for e in entities.values()}
        for pl in (0,1):
            checks += [step('assert_life',player=pl,value=life[pl]),step('assert_zone',player=pl,zone='battlefield',value=sum(e['alive'] and e['player']==pl for e in entities.values()))]
            if v != 'cleanup': checks += [step('assert_mana',player=pl,color=c,value=n) for c,n in enumerate(mana[pl])]
            for z, players in zones.items():
                ns = players[pl];checks.append(step('assert_zone',player=pl,zone=z,value=len(ns)))
                if z == 'library' and not (kind.startswith('scry') and owner == 1 and pl == owner): checks += [step('assert_zone_card',player=pl,zone=z,index=i,value=registry[nm]['csvid']) for i,nm in enumerate(ns)]
                else:
                    counts=Counter(ns);checks += [step('assert_zone_multiplicity',player=pl,zone=z,index=registry[nm]['csvid'],value=counts[nm]) for nm in sorted(universe)]
        seq = pre+actions+checks
        if v in ('ready_snapshot','pending_snapshot','mana_snapshot'):
            cut = 0 if v == 'ready_snapshot' else pending_cut if v == 'pending_snapshot' else mana_cut
            tail = actions[cut:]+checks
            seq = pre+actions[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        if v == 'resolved_snapshot': seq += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(checks)+[step('assert_recorded_state')]
        result.append(dict(name=f'LandEntryManaRecipe_{csvid}_{variant}',card=name,fixture='empty_main' if bare or v in ('short_library','empty_library') else 'stocked_zones',setup=setup,steps=seq,seed=123,
                           _recipe=dict(family=p['family'],version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],untested_rules=p['untested_rules'],untested_components=['land play allowances and timing','other replacement effects','additional trigger ordering','multiplayer opponent counts']+(['cycling interactions'] if p['untested_rules'] else []))))
        if bare or v in ('short_library','empty_library'):
            for z,ns in (('hand',HAND),('graveyard',GRAVE),('exile',EXILE),('library',LIBRARY)):
                for pl in (0,1):
                    # Custom fixtures must provision the original populated zones;
                    # final model zones already include costs and entry receipts.
                    initial=[] if bare and z!='library' else list(ns)
                    if z=='library' and pl==owner and v in ('short_library','empty_library'): initial=['Forest'] if v=='short_library' else []
                    setup[:0]=[card(nm,f'p{pl}_{z}_{i}',pl,z) for i,nm in enumerate(initial)]
    return result
