"""Typed, whole-Oracle entry library selection using immutable lifecycle receipts.

The exchange renderer supplies ordinary cast/payment/reentry/snapshot actions.
A separate Oracle ledger owns every selection and library destination receipt;
no source record or source type is rewritten to fit an older public grammar.
"""
import copy
import json
import re
from collections import Counter
from pathlib import Path

import hand_exchange_recipes as exchange
from library_search_recipes import eligible as library_eligible

FAMILY = 'entry_trigger_value'
DATA = Path(__file__).parent / 'data/entry_trigger_value_wave8_selection_profiles.json'
NUMBERS = {'two': 2, 'three': 3, 'four': 4}
SCOPES = {'an instant or sorcery': 'instant_or_sorcery', 'an artifact': 'artifact',
          'a creature': 'creature', 'a land': 'land',
          'a creature, land, or planeswalker': 'creature_land_planeswalker', 'Goblin': 'goblin',
          'a Human': 'human', 'Soldier': 'soldier', 'Zombie': 'zombie', 'Kavu': 'kavu',
          'Elf': 'elf', 'Merfolk': 'merfolk', 'Island': 'island'}
SUBTYPES = {'human': 'Human', 'soldier': 'Soldier', 'zombie': 'Zombie', 'kavu': 'Kavu',
            'elf': 'Elf', 'merfolk': 'Merfolk', 'island': 'Island'}
SUBTYPE_WITNESSES = {
    'human': ['Champion of the Parish', 'Crib Swap', 'Nameless Inversion'],
    'soldier': ['Champion of the Parish', "Veteran's Armaments", 'Crib Swap', 'Nameless Inversion'],
    'zombie': ['Scathe Zombies', 'Crib Swap', 'Nameless Inversion'],
    'kavu': ['Horned Kavu', 'Crib Swap', 'Nameless Inversion'],
    'elf': ['Elvish Mystic', 'Elvish Promenade', 'Crib Swap', 'Nameless Inversion'],
    'merfolk': ['Lord of Atlantis', "Aquitect's Will", 'Crib Swap', 'Nameless Inversion'],
    'island': ['Island', 'Tropical Island']}
PALETTE = ['Forest', 'Island', 'Grizzly Bears', 'Lightning Bolt', 'Divination',
           'Counterspell', 'Darksteel Relic', 'Jace Beleren', "Mons's Goblin Raiders", 'Tarfire']
ZONE_OPS = {'assert_zone_card', 'assert_zone_contains', 'assert_zone_multiplicity',
            'assert_zone_multiplicity_max', 'assert_hand_graveyard_total'}


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    types = set(record.get('type_line', '').partition(' — ')[0].split()) if record else set()
    if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not types & {'Creature', 'Artifact', 'Enchantment', 'Land'}
            or {'Aura', 'Equipment', 'Token'} & set(record.get('type_line', '').split())):
        raise ValueError('selection entry requires an ordinary standalone permanent')
    if 'Creature' in types and (any(not str(record.get(k, '')).isdigit() for k in ('power', 'toughness'))
                                or int(record['toughness']) <= 0):
        raise ValueError('selection entry creature must have fixed surviving stats')
    prefix = r'When (?:this (?:creature|artifact|enchantment|land)|' + re.escape(record['name']) + r') enters, '
    clauses = [line for line in record['oracle_text'].splitlines() if re.match(prefix, line)]
    if len(clauses) != 1: raise ValueError('selection entry requires one complete ordinary entry clause')
    body = re.sub(prefix, '', clauses[0], count=1)
    p = None
    match = re.fullmatch(r"look at the top (two|three|four) cards of (your|target player's) library, then put them back in any order\.", body)
    if match:
        p = dict(kind='reorder', window=NUMBERS[match[1]], scope='any', optional=False,
                 remainder='top', target_player=match[2] != 'your')
    match = re.fullmatch(r'look at the top (two|three|four) cards of your library[.,] (?:then )?[Pp]ut one of them into your hand and (?:the other|the rest) (on the bottom of your library(?: in any order)?|into your graveyard)\.', body)
    if match:
        p = dict(kind='one', window=NUMBERS[match[1]], scope='any', optional=False,
                 remainder='graveyard' if match[2] == 'into your graveyard' else 'bottom', target_player=False)
    match = re.fullmatch(r'look at the top (two|three|four) cards of your library\. You may reveal (.+) card from among them and put it into your hand\. Put the rest on the bottom of your library in any order\.', body)
    if match and match[2] in SCOPES:
        p = dict(kind='one', window=NUMBERS[match[1]], scope=SCOPES[match[2]], optional=True,
                 remainder='bottom', target_player=False)
    if body == 'reveal the top four cards of your library. You may put a land card from among them into your hand. Put the rest into your graveyard.':
        p = dict(kind='one', window=4, scope='land', optional=True, remainder='graveyard', target_player=False)
    match = re.fullmatch(r'reveal the top (two|three|four) cards of your library\. Put all (.+) cards revealed this way into your hand and the rest on the bottom of your library in any order\.', body)
    if match and match[2] in SCOPES:
        p = dict(kind='all', window=NUMBERS[match[1]], scope=SCOPES[match[2]], optional=False,
                 remainder='bottom', target_player=False)
    if p is None: raise ValueError('unsupported complete selection entry clause')
    p['clause'] = clauses[0]
    p['gaps'] = [line for line in record['oracle_text'].splitlines() if line != clauses[0]]
    return p


def spec(d, record):
    from recipes import cost_from_oracle
    if (d.get('family') != FAMILY or type(d.get('version')) is not int or d['version'] != 2
            or not isinstance(d.get('variants'), list) or not d['variants']
            or len(set(d['variants'])) != len(d['variants'])):
        raise ValueError('selection entry requires version two and distinct explicit variants')
    params = d.get('params')
    if not isinstance(params, dict) or set(params) != {'schema', 'mechanism', 'presentation'} or type(params['schema']) is not int or params['schema'] != 3:
        raise ValueError('selection entry requires explicit schema, mechanism and presentation')
    parsed = mechanism(record)
    profile = pins()['sources'].get(record['name'])
    if (not profile or profile['oracle'] != record or d.get('oracle_text') != record['oracle_text']
            or params['mechanism'] != parsed or profile['mechanism'] != parsed
            or params['presentation'] != profile['presentation']):
        raise ValueError('selection entry whole source pin or reviewed parameters changed')
    presentation = params['presentation']
    if (not isinstance(presentation, dict) or set(presentation) not in ({'selector'}, {'selector', 'human_selected_reveal'})
            or 'human_selected_reveal' in presentation and (presentation['human_selected_reveal'] is not True or not parsed['optional'] or parsed['kind'] != 'one')
            or presentation['selector'] not in ('typed', 'display', 'automatic', 'reorder')
            or (parsed['kind'] == 'reorder') != (presentation['selector'] == 'reorder')
            or (parsed['kind'] == 'all') != (presentation['selector'] == 'automatic')):
        raise ValueError('unreviewed selection input presentation')
    types = set(record['type_line'].partition(' — ')[0].split())
    land = 'Land' in types; creature = 'Creature' in types
    if land and record.get('mana_cost'): raise ValueError('ordinary land cannot have a cast mana cost')
    return dict(parsed, selection=True, family=FAMILY, source_name=record['name'], csvid=profile['csvid'],
                presentation=copy.deepcopy(presentation), land=land, creature=creature,
                mana=[0]*6 if land else cost_from_oracle(record), legendary='Legendary' in types,
                power=int(record['power']) if creature else 0, toughness=int(record['toughness']) if creature else 0)


def matches(record, scope):
    if scope == 'any': return True
    types = set(record['type_line'].partition(' — ')[0].split())
    if scope == 'land': return 'Land' in types
    if scope == 'creature_land_planeswalker': return bool(types & {'Creature', 'Land', 'Planeswalker'})
    if scope in SUBTYPES:
        # "Human/Soldier/etc cards" includes Kindred noncreatures. Changeling
        # supplies creature subtypes in every zone, never a land subtype.
        subtypes = record['type_line'].partition(' — ')[2].split()
        return SUBTYPES[scope] in subtypes or (scope != 'island' and 'Changeling' in record.get('keywords', []))
    return library_eligible(record, dict(scope=scope, limit=None))


def predicate_palette(p):
    # Keep the accepted generic palettes/receipts byte-identical. New subtype
    # fixtures alone gain independently pinned matching witnesses.
    return list(dict.fromkeys(PALETTE + SUBTYPE_WITNESSES.get(p['scope'], [])))


def core(p):
    return dict(mode='entry', recipient='self', operations=[],
                cost=dict(mana='', tap=False, sacrifice=False, pay_life=0, discard=0), keywords=[], mana_ability=False,
                family='hand_exchange', activation_mana=[0]*6, mana=p['mana'], creature=p['creature'], land=p['land'],
                legendary=p['legendary'], power=p['power'], toughness=p['toughness'], untested_rules=p['gaps'], instant=False)


def variants(p):
    modes = ['effect', 'payment', 'cleanup', 'empty_library', 'short_library', 'one_card_library', 'duplicates',
             'ready_snapshot', 'pending_snapshot', 'resolved_snapshot', 'bounce_recast', 'source_destroyed', 'nonmatching_entry']
    if not p['land']: modes += ['insufficient_mana', 'wrong_color', 'countered_cast']
    if p['creature']: modes += ['blink_source']
    if p['kind'] == 'reorder': modes += ['reorder']
    else: modes += ['alternate_choice', 'all_match']
    if p['scope'] != 'any': modes += ['no_match', 'outside_window']
    if p['optional']: modes += ['decline', 'predicate_match', 'predicate_no_match']
    if p['target_player']: modes += ['opponent_target']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes]


def validate_witnesses(oracle, rows):
    data = pins()
    for name, record in data['witnesses'].items():
        if oracle.get(name) != record or name not in rows:
            raise ValueError('selection witness full pin changed: ' + name)
    exchange.validate_witnesses(oracle, rows)
    for name in {name for names in SUBTYPE_WITNESSES.values() for name in names}:
        record = data['witnesses'].get(name)
        if (not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
                or rows[name]['kind'] != 'card' or rows[name]['handler'] in ('NULL', 'card_dummy', None)):
            raise ValueError('subtype witness must be an implemented normal primary: ' + name)
    for name, profile in data['sources'].items():
        if (oracle.get(name) != profile['oracle'] or name not in rows or rows[name]['csvid'] != profile['csvid']
                or rows[name]['kind'] != 'card' or rows[name]['handler'] in ('NULL', 'card_dummy', None)):
            raise ValueError('selection source pin or normal primary implementation changed: ' + name)


def library_fixture(p, mode, oracle):
    palette = predicate_palette(p)
    yes = [name for name in palette if matches(oracle[name], p['scope'])]
    no = [name for name in palette if not matches(oracle[name], p['scope'])]
    n = p['window']
    if mode == 'empty_library': return []
    if mode == 'predicate_match': return [yes[0]]
    if mode == 'predicate_no_match': return [no[0]]
    if p['scope'] == 'instant_or_sorcery': top = ['Lightning Bolt', 'Forest', 'Divination', 'Grizzly Bears'][:n]
    elif p['scope'] == 'creature_land_planeswalker': top = ['Grizzly Bears', 'Forest', 'Jace Beleren'][:n]
    elif p['scope'] == 'land': top = ['Forest', 'Counterspell', 'Island', 'Grizzly Bears'][:n]
    elif p['scope'] == 'goblin': top = ['Tarfire', 'Island', "Mons's Goblin Raiders", 'Grizzly Bears'][:n]
    elif p['scope'] == 'island': top = ['Island', 'Forest', 'Tropical Island'][:n]
    elif p['scope'] in SUBTYPES:
        witnesses = SUBTYPE_WITNESSES[p['scope']]
        top = [witnesses[0], 'Counterspell', witnesses[1], 'Grizzly Bears'][:n]
    else: top = ['Forest', 'Grizzly Bears', 'Lightning Bolt', 'Darksteel Relic'][:n]
    if mode in ('no_match', 'outside_window'): top = [no[i % len(no)] for i in range(n)]
    if mode == 'all_match': top = [yes[i % len(yes)] for i in range(n)]
    if mode == 'duplicates': top = [yes[0]] * n
    if mode == 'one_card_library': return top[:1]
    if mode == 'short_library': return top[:n-1]
    tail = ['Mountain', 'Darksteel Relic', 'Island', 'Giant Growth', 'Swamp']
    if mode == 'outside_window': tail.insert(0, yes[0])
    return top + tail


class SelectionLedger:
    def __init__(self, case, p, mode, owner, rows, oracle):
        self.p, self.mode, self.owner, self.rows, self.oracle = p, mode, owner, rows, oracle
        self.zones = {zone:[[], []] for zone in ('hand', 'graveyard', 'exile', 'library')}
        self.entities = {}
        for item in case['setup']:
            self.entities[item['ref']] = dict(name=item['name'], player=item['player'], zone=item['zone'], tapped=False)
            if item['zone'] != 'battlefield': self.zones[item['zone']][item['player']].append(item['name'])
        self.unordered_prefix = [0, 0]
        self.pending = None
        self.entered = 0

    def state(self):
        return copy.deepcopy((self.zones, self.entities, self.unordered_prefix, self.pending, self.entered))

    def restore(self, saved):
        self.zones, self.entities, self.unordered_prefix, self.pending, self.entered = copy.deepcopy(saved)

    def move(self, ref, destination):
        entity = self.entities[ref]; old = entity['zone']; player = entity['player']; name = entity['name']
        if old in self.zones: self.zones[old][player].remove(name)
        if destination in self.zones: self.zones[destination][player].append(name)
        entity['zone'] = destination

    def count(self, player, zone):
        if zone == 'battlefield': return sum(e['zone'] == zone and e['player'] == player for e in self.entities.values())
        return len(self.zones[zone][player])

    def selection(self):
        from recipes import step
        p = self.p; chooser = self.owner; target = 1-chooser if self.mode == 'opponent_target' else chooser
        deck = self.zones['library'][target]; window = list(deck[:p['window']]); n = len(window)
        choices = [step('choose_player', player=chooser, value=target)] if p['target_player'] else []
        if p['kind'] == 'reorder':
            if n == 1: choices.append(step('choose_zone', player=chooser, value=-1))
            elif n > 1 and chooser == 0:
                if self.mode in ('reorder', 'opponent_target'):
                    choices += [step('choose_zone_card', player=chooser, value=self.rows[name]['csvid']) for name in window[1:]]
                    deck[:n] = list(reversed(window))
                choices.append(step('choose_zone', player=chooser, value=-1))
            elif n > 1: self.unordered_prefix[target] = max(self.unordered_prefix[target], n)
            return choices
        eligible = [name for name in window if matches(self.oracle[name], p['scope'])]
        decline = self.mode in ('decline', 'predicate_match', 'predicate_no_match')
        chosen = list(eligible) if p['kind'] == 'all' else eligible[-1:] if self.mode == 'alternate_choice' else eligible[:1]
        if decline: chosen = []
        if p['kind'] == 'one' and n:
            if chosen:
                choices.append(step('choose_zone_card', player=chooser, value=self.rows[chosen[0]]['csvid']))
                if chooser == 0 and p['presentation'].get('human_selected_reveal'):
                    # reveal_chosen calls reveal_card_iid before moving the
                    # chosen card or asking about the remaining cards. AI's
                    # exact informational notice already has a host default.
                    choices.append(step('choose_dialog', player=chooser, value=0))
            else: choices.append(step('choose_optional_zone', player=chooser, value=self.rows[(eligible or window)[0]]['csvid']))
        rest = list(window)
        for name in chosen: rest.remove(name)
        del deck[:n]
        self.zones['hand'][target].extend(chosen)
        if p['remainder'] == 'graveyard': self.zones['graveyard'][target].extend(rest)
        else:
            # The real bottom helper asks in order until just one remains.
            choices += [step('choose_zone_card', player=target, value=self.rows[name]['csvid']) for name in rest[:-1]]
            deck.extend(rest)
        return choices

    def receipts(self):
        from recipes import step
        seq = [step('assert_forced_zone_choices', value=0)]
        universe = sorted({item['name'] for item in self.entities.values()} | set(predicate_palette(self.p)))
        for player in (0, 1):
            seq += [step('assert_shuffle_count', player=player, value=0),
                    step('assert_zone', player=player, zone='battlefield', value=self.count(player, 'battlefield'))]
            for zone, players in self.zones.items():
                names = players[player]; counts = Counter(names)
                seq.append(step('assert_zone', player=player, zone=zone, value=len(names)))
                seq += [step('assert_zone_multiplicity', player=player, zone=zone, index=self.rows[name]['csvid'], value=counts[name]) for name in universe]
                if zone == 'library':
                    seq += [step('assert_zone_card', player=player, zone=zone, index=i, value=self.rows[name]['csvid']) for i,name in enumerate(names) if i >= self.unordered_prefix[player]]
            totals = Counter(self.zones['hand'][player] + self.zones['graveyard'][player])
            seq += [step('assert_hand_graveyard_total', player=player, index=self.rows[name]['csvid'], value=totals[name]) for name in universe]
        return seq


def expand(name, csvid, d, p, selected, rows, oracle):
    from recipes import card, step, cost_from_oracle
    if csvid != p['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected)-set(variants(p)):
        raise ValueError('selection requires matching source and supported distinct variants')
    validate_witnesses(oracle, rows)
    output = []
    for variant in selected:
        mode,owner = variant.rsplit('_', 1); owner = int(owner)
        underlying = mode if mode in ('effect','payment','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','bounce_recast','source_destroyed','insufficient_mana','wrong_color','countered_cast','blink_source') else 'effect'
        case = exchange.expand(name, csvid, dict(d,target_choices=0), core(p), [f'{underlying}_{owner}'], rows, oracle)[0]
        case['setup'] = [item for item in case['setup'] if item['zone'] != 'library']
        library = library_fixture(p, mode, oracle)
        for player in (0,1):
            case['setup'] += [card(n,f'selection_library_{player}_{i}',player,'library') for i,n in enumerate(library)]
        if mode == 'nonmatching_entry':
            case['setup'].append(card('Hill Giant','nonmatching',owner,'hand'))
            case['steps'] += [step('cast',ref='nonmatching',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
            expected = [20-a-b for a,b in zip(p['mana'],cost_from_oracle(oracle['Hill Giant']))]
            case['steps'] += [step('assert_mana',player=owner,color=i,value=value) for i,value in enumerate(expected)]
        ledger = SelectionLedger(case,p,mode,owner,rows,oracle)
        transformed = []; saved = None; casts = 0
        for operation in case['steps']:
            current = dict(operation); op = current['op']; ref = current.get('ref')
            if op == 'capture': saved = ledger.state()
            if op == 'restore': ledger.restore(saved)
            if op == 'cast' and ref in ('source','nonmatching'):
                if ref == 'source':
                    casts += 1
                    if p['land'] and mode == 'bounce_recast' and casts == 2:
                        # Two real turn gates return to the owner's later turn.
                        transformed += [step('advance_turn'),step('advance_turn')]
                if current['value']:
                    ledger.move(ref,'stack'); ledger.pending = 'source' if ref == 'source' else 'other'
                elif mode == 'countered_cast': ledger.move(ref,'graveyard')
            if op == 'activate' and ref in ('blinker','bouncer'): ledger.pending = 'blink' if ref == 'blinker' else 'bounce'
            if op == 'destroy' and ref in ledger.entities: ledger.move(ref,'graveyard')
            if op == 'resolve' and ledger.pending:
                pending = ledger.pending; ledger.pending = None
                if pending == 'bounce': ledger.move('source','hand')
                elif pending == 'other': ledger.move('nonmatching','battlefield')
                else:
                    if pending == 'source': ledger.move('source','battlefield')
                    ledger.entities['source']['tapped'] = bool(p['land'] or pending == 'blink')
                    transformed += ledger.selection(); ledger.entered += 1
                transformed.append(current)
                transformed += ledger.receipts()
                if mode in ('predicate_match','predicate_no_match'):
                    inspected = library[0]
                    transformed += [step('assert_offered_count',value=1),step('assert_offered_selection_required',value=0),
                                    step('assert_offered_legal_card',index=rows[inspected]['csvid'],value=int(matches(oracle[inspected],p['scope'])))]
                continue
            if op in ZONE_OPS: continue
            if op == 'assert_zone':
                current['value'] = ledger.count(current['player'],current['zone'])
                if current['zone'] == 'hand' and ledger.pending == 'source' and current['player'] == owner: current['value'] += 1
            if op in ('assert_card','require_card') and ref == 'source' and current.get('property') == 'tapped':
                current['value'] = int(ledger.entities['source']['tapped'])
            transformed.append(current)
            if op == 'cast' and current['value'] and ref == 'source':
                transformed.append(step('assert_card',ref='source',property='in_hand',value=0))
        case['steps'] = [step('observe_search_filters',value=1)] + transformed + ledger.receipts()
        case['name'] = f'EntryTriggerSelectionRecipe_{csvid}_{variant}'
        case['_recipe'] = dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],
                               entry_clause=p['clause'],untested_components=p['gaps'] + ['multiplayer','replacement effects','control changes','land-play timing/quota rejection','separate priority between entry and trigger resolution'])
        output.append(case)
    return output
