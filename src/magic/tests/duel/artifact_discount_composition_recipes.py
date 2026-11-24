"""Real affinity and static cost layers, with paid cast-origin controls."""
import json
import re
from collections import Counter
from pathlib import Path

from artifact_affinity_recipes import RULE as AFFINITY, types
from library_search_recipes import wave6_exact as exact_pin


FAMILY = 'artifact_discount_composition'
DATA = Path(__file__).parent / 'data/artifact_discount_composition_wave8_profiles.json'
FLAT = 'Artifact spells you cast cost {1} less to cast.'
GRANT = 'Artifact creature spells you cast have affinity for artifacts. (They cost {1} less to cast for each artifact you control.)'
HAND_EXILE = 'When this creature enters, if you cast it from your hand, exile all artifacts.'
RELIC = 'Darksteel Relic'
WITNESSES = {RELIC, 'Darksteel Citadel', 'Renegade Map', 'Golem Artisan', 'Myr Enforcer', 'Myr Sire',
             'Mind Stone', 'Tidehollow Strix', 'Sylvan Advocate', 'Counterspell', 'Unsummon', 'Zombify',
             'Cloudshift', 'Act on Impulse', 'Elvish Mystic', 'Nessian Asp', 'Welkin Tern', 'Forest', 'Island'}
COMMON = ('full_price', 'short', 'countered', 'paid_replay')
OWN_AFFINITY = ('own_exact', 'opponent_only', 'nonartifact_only', 'off_battlefield', 'artifact_land', 'zero_generic')
PERMANENT_DISCOUNT = ('artifact_creature', 'artifact_noncreature', 'nonartifact', 'opposing_caster',
                      'colored_floor', 'wrong_colored_floor', 'source_leaves', 'multiple_reducers')
EXILE = ('mixed_exile', 'wrong_colored_floor', 'reanimate', 'blink', 'cast_from_exile', 'exile_permission_replay',
         'flying_evasion', 'flying_reach', 'blocks_flight', 'exile_origin_surplus')
EXILE_CASTS = {'cast_from_exile', 'exile_permission_replay', 'exile_origin_surplus'}
GOLEM = ('intrinsic_affinity', 'two_golems', 'flat_composition')


def pins():
    return json.loads(DATA.read_text(encoding='utf8'))


def fixed_cost(record):
    from recipes import cost_from_oracle

    # Ordinary shared fee vectors cover printed totals through ten.
    large_generic = re.fullmatch(r'\{(1[1-9]|20)\}', record['mana_cost'])
    return [int(large_generic[1]), 0, 0, 0, 0, 0] if large_generic else cost_from_oracle(record)


def mechanism(record):
    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or record.get('card_faces')
            or types(record) not in (['Creature'], ['Artifact', 'Creature'])
            or any(not re.fullmatch(r'[0-9]+', record.get(stat, '')) for stat in ('power', 'toughness'))):
        raise ValueError('whole normal creature with fixed printed characteristics required')
    clauses = record['oracle_text'].splitlines()
    if clauses == [AFFINITY, 'Flying', HAND_EXILE] and types(record) == ['Creature']:
        role, keywords = 'hand_exile', {'Affinity', 'Flying'}
    elif clauses == [AFFINITY, GRANT] and types(record) == ['Artifact', 'Creature']:
        role, keywords = 'grant', {'Affinity'}
    elif clauses == [FLAT] and types(record) == ['Artifact', 'Creature']:
        role, keywords = 'flat', set()
    else:
        raise ValueError('unknown complete artifact discount and secondary clauses')
    if set(record['keywords']) != keywords:
        raise ValueError('printed keyword metadata changed')
    return dict(printed=fixed_cost(record), role=role, clauses=clauses, artifact='Artifact' in types(record),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(shape):
    role = shape['role']
    modes = COMMON
    if role != 'flat':
        modes += OWN_AFFINITY
    if role == 'hand_exile':
        modes += EXILE
    else:
        modes += PERMANENT_DISCOUNT
    if role == 'grant':
        modes += GOLEM
    return [f'{mode}_{owner}' for mode in modes for owner in (0, 1)]


def spec(declaration, record):
    shape = mechanism(record)
    source = pins()['sources'].get(record['name'])
    params = declaration.get('params')
    if (not source or not exact_pin(source['oracle'], record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not isinstance(params, dict)
            or set(params) != {'schema', 'mechanism'} or type(params['schema']) is not int or params['schema'] != 1
            or not exact_pin(params['mechanism'], shape)):
        raise ValueError('whole pinned artifact-discount record and exact price required')
    return dict(shape, csvid=source['csvid'], family=FAMILY)


def validate_witnesses(oracle, rows):
    data = pins()
    if set(data) != {'sources', 'witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete independent discount witness pool required')
    for group in ('sources', 'witnesses'):
        for name, pin in data[group].items():
            row = rows.get(name, {})
            if (not exact_pin(pin['oracle'], oracle.get(name)) or row.get('csvid') != pin['csvid']
                    or row.get('handler') != pin['handler'] or row.get('kind') != 'card'
                    or row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError('pinned implemented source or witness changed: ' + name)
            if group == 'sources':
                mechanism(oracle[name])


class Game:
    def __init__(self, name, shape, mode, owner, rows, oracle):
        self.name, self.shape, self.mode = name, shape, mode
        self.owner, self.foe = owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.steps, self.setup, self.cards = [], [], {}
        self.pools, self.life = [[0]*6 for _ in (0, 1)], [20, 20]
        self.build()

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def add(self, name, ref, player, zone='battlefield', *, tapped=False):
        from recipes import card

        item = card(name, ref, player, zone)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.cards[ref] = dict(item, tapped=tapped, damage=0)

    def build(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        from_exile = mode in EXILE_CASTS
        self.add(self.name, 'source', owner, 'graveyard' if mode == 'reanimate' else 'library' if from_exile else 'hand')
        for player in (0, 1):
            for index, name in enumerate(('Forest', 'Island', 'Forest')):
                self.add(name, f'library_{player}_{index}', player, 'library')
        relics = {'own_exact': 2, 'short': 2, 'paid_replay': 2, 'zero_generic': self.shape['printed'][0]+1,
                  'wrong_colored_floor': self.shape['printed'][0]+1, 'two_golems': 4, 'mixed_exile': 2,
                  'artifact_creature': 2, 'intrinsic_affinity': 2, 'artifact_noncreature': 2, 'colored_floor': 2,
                  'cast_from_exile': 2, 'exile_permission_replay': 2, 'exile_origin_surplus': 2, 'reanimate': 1, 'blink': 1,
                  'flat_composition': 1}.get(mode, 0)
        for index in range(relics):
            self.add(RELIC, f'own_artifact_{index}', owner, tapped=index == 0)
        if from_exile or mode in ('opponent_only', 'mixed_exile', 'reanimate', 'opposing_caster'):
            self.add('Myr Sire', 'enemy_artifact_creature', foe)
            self.add(RELIC, 'enemy_relic', foe)
        if mode in ('mixed_exile', 'nonartifact_only'):
            self.add('Sylvan Advocate', 'ordinary_creature', owner)
            self.add('Forest', 'ordinary_land', foe)
        if mode in ('mixed_exile', 'artifact_land'):
            self.add('Darksteel Citadel', 'artifact_land', owner, tapped=True)
        if mode == 'off_battlefield':
            for zone in ('hand', 'graveyard', 'exile', 'library'):
                self.add(RELIC, f'artifact_{zone}', owner, zone)
        support = {
            'countered': 'Counterspell', 'source_leaves': 'Unsummon', 'reanimate': 'Zombify',
            'blink': 'Cloudshift', 'cast_from_exile': 'Act on Impulse', 'exile_permission_replay': 'Act on Impulse',
            'exile_origin_surplus': 'Act on Impulse',
        }
        if mode in support:
            self.add(support[mode], 'support', foe if mode == 'countered' else owner, 'hand')
        probes = {'artifact_creature': 'Golem Artisan', 'artifact_noncreature': 'Mind Stone', 'nonartifact': 'Sylvan Advocate',
                  'opposing_caster': 'Golem Artisan', 'colored_floor': 'Tidehollow Strix', 'source_leaves': 'Golem Artisan',
                  'multiple_reducers': 'Golem Artisan', 'intrinsic_affinity': 'Myr Enforcer', 'flat_composition': 'Golem Artisan',
                  'two_golems': self.name, 'blink': 'Renegade Map'}
        if self.shape['role'] != 'hand_exile' and mode == 'wrong_colored_floor':
            probes[mode] = 'Tidehollow Strix'
        if mode in probes:
            self.add(probes[mode], 'probe', foe if mode == 'opposing_caster' else owner, 'hand')
        if mode == 'multiple_reducers':
            alternate = 'Foundry Inspector' if self.name == 'Etherium Sculptor' else 'Etherium Sculptor'
            self.add(self.name if self.shape['role'] == 'grant' else alternate, 'reducer', owner)
        if mode == 'flat_composition':
            self.add('Foundry Inspector', 'reducer', owner)
        if mode in ('flying_evasion', 'flying_reach', 'blocks_flight'):
            blocker = {'flying_evasion': 'Elvish Mystic', 'flying_reach': 'Nessian Asp', 'blocks_flight': 'Welkin Tern'}[mode]
            self.add(blocker, 'combatant', foe)

    def fund(self, player, amounts):
        self.pools[player] = list(amounts)
        for color, amount in enumerate(amounts):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pay(self, player, price):
        result = [have-due for have, due in zip(self.pools[player], price)]
        if min(result) < 0:
            raise ValueError('illegal real fixture payment')
        self.pools[player] = result

    def observe_mana(self):
        for player, pool in enumerate(self.pools):
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=amount)

    def price(self, ref):
        item = self.cards[ref]
        record, player = self.oracle[item['name']], item['player']
        cost = fixed_cost(record)
        battlefield = [self.oracle[card['name']] for card in self.cards.values()
                       if card['player'] == player and card['zone'] == 'battlefield']
        controlled_artifacts = sum('Artifact' in types(card) for card in battlefield)
        reduction = controlled_artifacts if AFFINITY in record['oracle_text'].splitlines() else 0
        if 'Artifact' in types(record):
            reduction += sum(card['oracle_text'] == FLAT for card in battlefield)
            if 'Creature' in types(record):
                reduction += controlled_artifacts * sum(GRANT in card['oracle_text'].splitlines() for card in battlefield)
        cost[0] = max(0, cost[0]-reduction)
        return cost

    def cast(self, ref, *, funding=None, allowed=True, response=False, target=None, zone_target=None):
        item = self.cards[ref]
        player = item['player']
        price = self.price(ref)
        self.fund(player, price if funding is None else funding)
        if target:
            self.emit('choose_target', player=player, ref=target)
        if zone_target:
            self.emit('choose_zone_card', player=player, value=self.rows[self.cards[zone_target]['name']]['csvid'])
        self.emit('cast_response' if response else 'cast', ref=ref, value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=(1+int(response)) if allowed else 0)
        if allowed:
            self.pay(player, price)
            item['zone'] = 'stack'
        self.observe_mana()

    def finish(self, ref, *, response=False, cast_from_hand=False):
        item = self.cards[ref]
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        item['zone'] = 'battlefield' if set(types(self.oracle[item['name']])) & {'Artifact', 'Creature'} else 'graveyard'
        item['tapped'] = item['name'] == 'Renegade Map'
        if ref == 'source' and self.shape['role'] == 'hand_exile' and cast_from_hand:
            for entry in self.cards.values():
                if entry['zone'] == 'battlefield' and 'Artifact' in types(self.oracle[entry['name']]):
                    entry['zone'] = 'exile'

    def bind_source(self):
        self.emit('bind_permanent', player=self.owner, ref='source', value=self.shape['csvid'])

    def receipts(self, *, mana_last=False):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        if not mana_last:
            self.observe_mana()
        quantities = Counter((row['player'], row['zone'], self.rows[row['name']]['csvid']) for row in self.cards.values())
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ('battlefield', 'hand', 'graveyard', 'exile', 'library'):
                self.emit('assert_zone', player=player, zone=zone, value=sum(
                    count for (who, where, _), count in quantities.items() if who == player and where == zone))
        for (player, zone, index), value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity', player=player, zone=zone, index=index, value=value)
        for ref, row in self.cards.items():
            if row['zone'] != 'battlefield':
                continue
            record = self.oracle[row['name']]
            self.emit('assert_matching_permanents', player=row['player'], index=self.rows[row['name']]['csvid'],
                      property='artifact', value=int('Artifact' in types(record)))
            self.emit('assert_card', ref=ref, property='tapped', value=int(row['tapped']))
            if 'Creature' in types(record):
                for field, value in [('power', int(record['power'])), ('toughness', int(record['toughness'])), ('damage', row['damage'])]:
                    self.emit('assert_card', ref=ref, property=field, value=value)

        if mana_last:
            self.observe_mana()

    def cast_from_exile(self):
        self.cast('support')
        self.finish('support')
        for ref in ('source', f'library_{self.owner}_0', f'library_{self.owner}_1'):
            self.cards[ref]['zone'] = 'exile'
        self.receipts()
        price = self.price('source')
        full_budget = self.mode == 'exile_origin_surplus'
        self.fund(self.owner, fixed_cost(self.oracle[self.name]) if full_budget else price)
        expected = self.shape['csvid']
        self.emit('can_play_exiled', player=self.foe, index=expected, value=0)
        replay = len(self.steps) if self.mode == 'exile_permission_replay' else None
        self.emit('can_play_exiled', player=self.owner, index=expected, value=1)
        self.emit('play_exiled', player=self.owner, index=expected, value=1)
        self.pay(self.owner, price)
        self.finish('source')
        self.bind_source()
        self.emit('can_play_exiled', player=self.owner, index=expected, value=0)
        self.receipts(mana_last=full_budget)
        return replay

    def flying_combat(self):
        defense = self.mode == 'blocks_flight'
        attack, block = ('combatant', 'source') if defense else ('source', 'combatant')
        self.emit('advance_turn')
        if not defense:
            self.emit('advance_turn')
        self.emit('begin_attackers')
        self.emit('attack', ref=attack, value=1)
        self.cards[attack]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        legal = self.mode != 'flying_evasion'
        self.emit('block', ref=block, other=attack, value=int(legal))
        self.emit('advance_combat')
        self.emit('assert_card', ref=attack, property='blocked', value=int(legal))
        self.emit('combat_damage')
        self.emit('end_combat')
        power = int(self.oracle[self.cards[attack]['name']]['power'])
        if legal:
            for dealer, recipient in ((attack, block), (block, attack)):
                hit = int(self.oracle[self.cards[dealer]['name']]['power'])
                self.cards[recipient]['damage'] += hit
                if self.cards[recipient]['damage'] >= int(self.oracle[self.cards[recipient]['name']]['toughness']):
                    self.cards[recipient]['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= power
        self.receipts()


def _case(name, csvid, declaration, shape, variant, rows, oracle):
    from recipes import COLORS, step

    mode, side = variant.rsplit('_', 1)
    owner = int(side)
    world = Game(name, shape, mode, owner, rows, oracle)
    emit = world.emit
    emit('turn', player=owner, value=0)
    for player in (0, 1):
        world.fund(player, [0]*6)
    world.receipts()
    replay = None
    if mode == 'reanimate':
        world.cast('support', zone_target='source')
        world.finish('support')
        world.cards['source']['zone'] = 'battlefield'
        world.bind_source()
        world.receipts()
    elif mode in EXILE_CASTS:
        replay = world.cast_from_exile()
    else:
        price = world.price('source')
        denied = mode in ('short', 'opponent_only', 'nonartifact_only', 'off_battlefield') or (
            mode == 'wrong_colored_floor' and shape['role'] == 'hand_exile')
        if denied and mode != 'wrong_colored_floor':
            price[0] -= 1
        if denied and mode == 'wrong_colored_floor':
            price[0], price[COLORS['R']] = 3, price[COLORS['R']]-1
        if mode == 'zero_generic':
            price[0] += 2
        if mode == 'paid_replay':
            replay = len(world.steps)
        world.cast('source', funding=price, allowed=not denied)
        if mode == 'countered':
            emit('announce_spell', ref='source')
            world.cast('support', response=True)
            world.finish('support', response=True)
            world.cards['source']['zone'] = 'graveyard'
        elif not denied:
            world.finish('source', cast_from_hand=True)
        world.receipts()

    if mode == 'source_leaves':
        world.cast('support', target='source')
        world.finish('support')
        world.cards['source']['zone'] = 'hand'
        world.receipts()
    if 'probe' in world.cards:
        if mode == 'opposing_caster':
            emit('advance_turn')
        price = world.price('probe')
        denied = mode in ('wrong_colored_floor', 'opposing_caster', 'source_leaves')
        if denied:
            if mode == 'wrong_colored_floor':
                price[0], price[COLORS['U']] = 4, 0
            else:
                price[0] -= 1
        world.cast('probe', funding=price, allowed=not denied)
        if not denied:
            world.finish('probe')
        world.receipts()
    if mode == 'blink':
        world.cast('support', target='source')
        world.finish('support')
        world.bind_source()
        world.receipts()
    if mode in ('flying_evasion', 'flying_reach', 'blocks_flight'):
        world.flying_combat()
    if replay is not None:
        suffix = world.steps[replay:]
        world.steps = (world.steps[:replay] + [step('capture')] + suffix
                       + [step('record_state'), step('restore'), step('ai_roundtrip')] + suffix + [step('assert_recorded_state')])
    metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                    oracle_text=declaration['oracle_text'], covered_clauses=shape['clauses'], untested_components=[
                        'Alternative free-from-hand effects and reducer control changes.',
                        'Artifact tokens, type-changing effects, and multiplayer.'])
    return dict(name=f'ArtifactDiscountCompositionRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=123,
                setup=world.setup, steps=world.steps, _recipe=metadata)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if csvid != shape['csvid'] or not selected or len(set(selected)) != len(selected) or set(selected) - set(variants(shape)):
        raise ValueError('distinct reviewed artifact discount variants required')
    validate_witnesses(oracle, rows)
    return [_case(name, csvid, declaration, shape, variant, rows, oracle) for variant in selected]
