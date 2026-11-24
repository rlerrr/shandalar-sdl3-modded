"""Whole-Oracle attack taxes through the production payment gate and combat."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact


FAMILY = 'global_attack_tax'
PROFILE = Path(__file__).parent / 'data/global_attack_tax_wave8_profiles.json'
FIXED = "Creatures can't attack you unless their controller pays {%s} for each creature they control that's attacking you."
SPHERE = "Creatures can't attack you or planeswalkers you control unless their controller pays {X} for each of those creatures, where X is the number of enchantments you control."
LIBRARY = ('Forest', 'Island', 'Plains', 'Grizzly Bears')
WITNESSES = set(LIBRARY) | {'Hill Giant', 'Jace Beleren', 'Fervor', 'Darksteel Relic', 'Naturalize', 'Murder', 'Giant Spider'}
COMMON = ('pay_exact', 'decline', 'underfunded', 'empty_pool', 'two_paid', 'first_only', 'second_only', 'stack_sources',
          'opposite_source', 'own_attacker', 'tapped_source', 'no_source', 'cast_source', 'removed_source',
          'tapped_attacker', 'sick_attacker', 'walker_no_source', 'walker_short')
SPHERE_MODES = ('own_enchantments', 'opposing_enchantments', 'nonenchantment', 'remove_enchantment', 'cast_enchantment', 'mixed_stack')
FLYING = {'flying_unblocked', 'flying_reach'}
WALKERS = {'walker_no_source', 'walker_short'}
TWO = {'two_paid', 'first_only', 'second_only'}
REQUIRED_WITNESSES = {'Jace Beleren', 'Naturalize', 'Murder', 'Fervor'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def _pilot_parameters(record):
    from recipes import cost_from_oracle

    printed = record.get('mana_cost', '')
    text = record.get('oracle_text', '')
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not re.fullmatch(r'\{[1-9]\}\{[UW]\}', printed)):
        raise ValueError('fixed-cost normal attack-tax source required')
    kind, keywords = record['type_line'], record['keywords']
    if text == SPHERE and kind == 'Enchantment' and keywords == []:
        return dict(cost=cost_from_oracle(record), tax_formula='enchantments', protects_walker=True, flying=False)
    for amount in (1, 2, 3):
        clause = FIXED % amount
        if text == clause and kind == 'Enchantment' and keywords == []:
            return dict(cost=cost_from_oracle(record), tax_formula='fixed', amount=amount, protects_walker=False, flying=False)
        if (text == 'Flying\n' + clause and kind.startswith('Creature — ') and keywords == ['Flying']
                and all(re.fullmatch('[1-9]', record.get(stat, '')) for stat in ('power', 'toughness'))):
            return dict(cost=cost_from_oracle(record), tax_formula='fixed', amount=amount, protects_walker=False, flying=True)
    raise ValueError('whole fixed player or dynamic player-plus-walker attack tax required')


def variants(params):
    if 'composition' in params:
        modes = {'domain': DOMAIN, 'vigilance': VIGILANCE, 'koskun': UPKEEP_KOSKUN,
                 'elephant': UPKEEP_ELEPHANT}[params['composition']]
        return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]
    modes = list(COMMON)
    if params['tax_formula'] == 'enchantments':
        modes.extend(SPHERE_MODES)
    if params['flying']:
        modes.extend(sorted(FLYING))
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(profile()['sources'].get(record['name']), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(params, declaration.get('params'))):
        raise ValueError('exact full Oracle and typed attack-tax recipe required')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed attack-tax variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    records = data['sources'] | data['witnesses']
    if set(data['witnesses']) != WITNESSES or set(records) != set(data['registry']):
        raise ValueError('full attack-tax source and witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if (not exact(oracle[name], record) or row['kind'] != 'card'
                or not exact(data['registry'][name], {k: row[k] for k in ('csvid', 'handler')})):
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in data['sources'].keys() | (REQUIRED_WITNESSES | {'Stone Rain', 'Concordant Crossroads', 'Colossus of Sardia'}) and row['handler'] in ('card_dummy', '0', 'NULL'):
            raise ValueError(f'expected production handler for {name}')


class Board:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.owner if self.mode in FLYING | {'own_attacker'} else self.enemy
        self.defender = 1 - self.active
        self.name, self.csvid, self.variant = name, csvid, variant
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.loyalty = None
        mode = self.mode
        source_hand = mode in ('no_source', 'cast_source', 'walker_no_source')
        self.add(name, 'source', self.owner, 'hand' if source_hand else 'battlefield', state='tapped' if mode == 'tapped_source' else 'untapped')
        self.first = 'source' if mode in FLYING else 'first'
        if self.first == 'first':
            state = {'tapped_attacker': 'tapped', 'sick_attacker': 'sick'}.get(mode, 'untapped')
            black_witnesses = {'black': 'Walking Corpse', 'black_multicolor': 'Golgari Guildmage'}
            actual_attacker = black_witnesses.get(mode, 'Grizzly Bears' if mode in WALKERS else 'Hill Giant')
            self.add(actual_attacker, 'first', self.active, state=state)
        if mode in TWO:
            self.add('Grizzly Bears', 'second', self.active)
        if mode in ('stack_sources', 'opposite_source'):
            self.add(name, 'other_source', self.owner if mode == 'stack_sources' else self.enemy)
        elif mode == 'mixed_stack':
            self.add('Ghostly Prison', 'other_source', self.owner)
        if mode in WALKERS:
            self.add('Jace Beleren', 'walker', self.defender, 'hand')
        if mode in ('own_enchantments', 'opposing_enchantments', 'remove_enchantment', 'cast_enchantment'):
            self.add('Fervor', 'enchantment', self.enemy if mode == 'opposing_enchantments' else self.owner,
                     'hand' if mode == 'cast_enchantment' else 'battlefield')
        if mode in ('own_enchantments', 'remove_enchantment'):
            self.add('Fervor', 'enchantment_two', self.owner)
        if mode == 'nonenchantment':
            self.add('Darksteel Relic', 'artifact', self.owner)
        if mode in ('removed_source', 'remove_enchantment'):
            self.add('Naturalize' if 'Creature' not in oracle[name]['type_line'] or mode == 'remove_enchantment' else 'Murder', 'removal', self.active, 'hand')
        if mode in FLYING:
            self.add('Grizzly Bears', 'ground', self.defender)
            if mode == 'flying_reach':
                self.add('Giant Spider', 'reach', self.defender)
        for player in (0, 1):
            self.add('Forest', f'grave_{player}', player, 'graveyard')
            self.add('Island', f'exile_{player}', player, 'exile')
            for i, card in enumerate(LIBRARY):
                self.add(card, f'lib_{player}_{i}', player, 'library')

    def add(self, name, ref, player, zone='battlefield', *, state='untapped'):
        from recipes import card

        item = card(name, ref, player, zone)
        if zone == 'battlefield':
            item['state'] = [state]
        if zone in ('hand', 'battlefield'):
            record = self.oracle[name]
            stats = tuple(int(record[key]) for key in ('power', 'toughness')) if 'power' in record else None
            self.entities[ref] = dict(name=name, player=player, zone=zone, stats=stats, tapped=state == 'tapped', damage=0)
        self.setup.append(item)
        self.zones[zone][player].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=int(value))

    def move(self, ref, destination):
        entity = self.entities[ref]
        name, player, zone = (entity[key] for key in ('name', 'player', 'zone'))
        self.zones[zone][player].remove(name)
        self.zones[destination][player].append(name)
        entity['zone'] = destination

    def fund(self, player, amounts):
        self.mana[player] = list(amounts)
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('set_mana', player=who, color=color, value=value)
        self.emit('refresh')

    def pools(self):
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=who, color=color, value=value)

    def receipt(self):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for ref, entity in self.entities.items():
            if entity['zone'] != 'battlefield':
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', entity['tapped'])
            if entity['stats'] is not None:
                for field, value in zip(('power', 'toughness', 'damage'), (*entity['stats'], entity['damage'])):
                    self.observe(ref, field, value)
        if self.loyalty is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)
        names = sorted({entity['name'] for entity in self.entities.values()})
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for zone, seats in self.zones.items():
                cards = seats[who]
                self.emit('assert_zone', player=who, zone=zone, value=len(cards))
                if zone == 'library':
                    for i, name in enumerate(cards):
                        self.emit('assert_zone_card', player=who, zone=zone, index=i, value=self.rows[name]['csvid'])
                else:
                    counts = Counter(cards)
                    for name in names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def cast(self, ref, *, target=None):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        record = self.oracle[entity['name']]
        player, fee = entity['player'], cost_from_oracle(record)
        self.fund(player, fee)
        if target:
            self.emit('choose_target', ref=target, player=player)
        self.emit('cast', ref=ref, value=1)
        self.emit('require_card', ref=ref, property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.mana[player] = [have - cost for have, cost in zip(self.mana[player], fee)]
        self.pools()
        self.emit('resolve')
        self.move(ref, 'graveyard' if target else 'battlefield')
        if target:
            self.move(target, 'graveyard')
        self.emit('refresh')
        if ref == 'walker':
            self.loyalty = int(record['loyalty'])
        self.receipt()

    def raw_taxes(self):
        if self.active == self.owner:
            return []
        taxes = []
        for ref in ('source', 'other_source'):
            entity = self.entities.get(ref)
            if not entity or entity['zone'] != 'battlefield' or entity['player'] != self.defender:
                continue
            param = classify(self.oracle[entity['name']])
            if self.mode in WALKERS and not param['protects_walker']:
                continue
            if param['tax_formula'] == 'fixed':
                taxes.append(param['amount'])
            elif param['tax_formula'] == 'domain':
                taxes.append(len(domain_types(self.oracle, self.zones['battlefield'][self.defender])))
            else:
                taxes.append(sum('Enchantment' in self.oracle[name]['type_line'] for name in self.zones['battlefield'][self.defender]))
        return taxes

    def begin_scene(self):
        mode = self.mode
        prep = {'cast_source': 'source', 'cast_enchantment': 'enchantment', 'walker_no_source': 'walker', 'walker_short': 'walker'}.get(mode)
        self.emit('turn', player=self.owner if prep else self.active, value=0)
        self.fund(self.active, [0] * 6)
        if prep:
            self.cast(prep)
            self.emit('advance_turn')
            self.emit('untap_step', player=self.active)
            self.emit('turn', player=self.active, value=0)
        if mode in ('removed_source', 'remove_enchantment'):
            self.cast('removal', target='source' if mode == 'removed_source' else 'enchantment')

    def paid_declaration(self):
        mode = self.mode
        taxes = self.raw_taxes()
        total = sum(taxes)
        if mode in ('empty_pool', 'walker_short'):
            budget = 0
        elif mode == 'underfunded':
            budget = total - 1
        elif mode == 'two_paid':
            budget = 2 * total
        else:
            budget = total
        self.fund(self.active, [budget, 0, 0, 0, 0, 0])
        self.emit('begin_attackers')
        committed = []
        for ref in ([self.first, 'second'] if mode in TWO else [self.first]):
            prohibited = mode in BLACK_MODES
            eligible = not prohibited and mode not in ('tapped_attacker', 'sick_attacker')
            decline = mode == 'decline' or mode == 'second_only' and ref == self.first
            enough = self.mana[self.active][0] >= total
            if mode in WALKERS:
                self.emit('choose_dialog', player=self.active, value=1)
            if prohibited:
                # An ordinary intent should remain unused when the real black
                # prohibition rejects the creature before a payment offer.
                self.emit('choose_dialog', player=self.active, value=0)
            if eligible and taxes and enough:
                for _ in (taxes[:1] if decline else taxes):
                    self.emit('choose_dialog', player=self.active, value=int(decline))
            success = eligible and enough and not decline
            self.emit('attack_with_tax', ref=ref, value=int(success))
            self.emit('assert_unused_choices', value=int(prohibited))
            if success:
                self.mana[self.active][0] -= total
                self.entities[ref]['tapped'] = 'Vigilance' not in self.oracle[self.entities[ref]['name']]['keywords']
                committed.append(ref)
            self.observe(ref, 'attacking', success)
            self.observe(ref, 'tapped', self.entities[ref]['tapped'])
            self.pools()
        self.emit('finish_attackers')
        return committed

    def combat(self, committed):
        self.emit('begin_blockers')
        if self.mode in FLYING:
            self.emit('block', ref='ground', other='source', value=0)
        reach = self.mode == 'flying_reach'
        if reach:
            self.emit('block', ref='reach', other='source', value=1)
        self.emit('advance_combat')
        if self.mode in FLYING:
            self.observe('source', 'blocked', reach)
        self.receipt()
        self.emit('combat_damage')
        if reach:
            muse, spider = [self.entities[ref] for ref in ('source', 'reach')]
            muse['damage'], spider['damage'] = spider['stats'][0], muse['stats'][0]
        else:
            damage = sum(self.entities[ref]['stats'][0] for ref in committed)
            if self.loyalty is None:
                self.life[self.defender] -= damage
            else:
                self.loyalty -= damage
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')
        self.emit('cleanup')
        for entity in self.entities.values():
            entity['damage'] = 0
        self.emit('refresh')
        self.receipt()

    def case(self):
        from recipes import step

        self.steps.append(step('capture'))
        self.begin_scene()
        self.receipt()
        self.combat(self.paid_declaration())
        suffix = self.steps[1:]
        run = self.steps + [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'GlobalAttackTaxRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=run, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Two supported player seats, one combat, no attacking battles or third player.',
                            'Mixed paid walker selection and simultaneous short multi-source payment are held for explicit UI calibration.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    composition = params.get('composition')
    scene = UpkeepBoard if composition in ('elephant', 'koskun') else CompositionBoard if composition else Board
    return [scene(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]


# Appended only in the private Domain and Vigilance preview.

DOMAIN_TEXT = "Domain — Creatures can't attack you unless their controller pays {X} for each creature they control that's attacking you, where X is the number of basic land types among lands you control."
VIGILANCE_TEXT = "Vigilance\nCreatures can't attack you or planeswalkers you control unless their controller pays {1} for each of those creatures."
DOMAIN = ('domain_zero', 'domain_one', 'domain_three', 'domain_five', 'domain_duplicate', 'domain_enemy', 'domain_dual',
          'domain_play', 'domain_remove', 'underfunded', 'two_paid', 'first_only', 'cast_source', 'walker_short')
VIGILANCE = ('pay_exact', 'decline', 'underfunded', 'two_paid', 'first_only', 'stack_distinct', 'opposite_source',
             'own_vigilance', 'vigilance_blocks_next', 'cast_source', 'removed_source', 'walker_short')
BASIC_TYPES = {'Swamp', 'Island', 'Forest', 'Mountain', 'Plains'}
COMPOSITION_WITNESSES = {'Tropical Island', 'Mountain', 'Swamp', 'Stone Rain'}
DOMAIN_BOARDS = {
    'domain_zero': (), 'domain_one': ('Forest',), 'domain_three': ('Forest', 'Island', 'Swamp'),
    'domain_five': ('Forest', 'Island', 'Swamp', 'Mountain', 'Plains'), 'domain_duplicate': ('Forest', 'Forest', 'Island'),
    'domain_enemy': (), 'domain_dual': ('Tropical Island',), 'domain_play': ('Forest',),
    'domain_remove': ('Forest', 'Island', 'Swamp'), 'underfunded': ('Forest', 'Island', 'Swamp'),
    'two_paid': ('Forest', 'Island', 'Swamp'), 'first_only': ('Forest', 'Island'),
    'cast_source': ('Forest',), 'walker_short': ('Forest',),
}


def composition_parameters(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete fixed normal attack-tax source required')
    printed, text = record['mana_cost'], record['oracle_text']
    kind, keywords = record['type_line'], record['keywords']
    if text == DOMAIN_TEXT and kind == 'Enchantment' and keywords == ['Domain'] and re.fullmatch(r'\{[1-9]\}\{U\}', printed):
        return dict(cost=cost_from_oracle(record), composition='domain', tax_formula='domain', protects_walker=False, flying=False)
    if (text == VIGILANCE_TEXT and kind.startswith('Legendary Creature — ') and keywords == ['Vigilance']
            and re.fullmatch(r'\{[1-9]\}\{W\}\{W\}', printed)
            and all(re.fullmatch('[1-9]', record.get(part, '')) for part in ('power', 'toughness'))):
        return dict(cost=cost_from_oracle(record), composition='vigilance', tax_formula='fixed', amount=1,
                    protects_walker=True, flying=False)
    raise ValueError('complete reviewed attack-tax composition required')


def domain_types(oracle, names):
    return {
        subtype
        for name in names
        if 'Land' in oracle[name]['type_line'].split(' — ')[0].split()
        for subtype in oracle[name]['type_line'].split(' — ')[-1].split()
        if subtype in BASIC_TYPES
    }


class CompositionBoard(Board):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        mode, seat = variant.rsplit('_', 1)
        vigilance = mode in ('own_vigilance', 'vigilance_blocks_next')
        board_variant = f'own_attacker_{seat}' if vigilance else variant
        super().__init__(name, csvid, declaration, params, board_variant, rows, oracle)
        self.mode, self.variant = mode, variant
        if vigilance:
            self.first = 'source'
        if mode == 'vigilance_blocks_next':
            self.add('Grizzly Bears', 'enemy_attacker', self.enemy)
        if mode == 'stack_distinct':
            self.add('Ghostly Prison', 'other_source', self.owner)
        if params['composition'] == 'domain':
            self.add_domain_witnesses()

    def add_domain_witnesses(self):
        for i, land in enumerate(DOMAIN_BOARDS[self.mode]):
            self.add(land, f'land_{i}', self.owner)
        if self.mode == 'domain_enemy':
            for i, land in enumerate(DOMAIN_BOARDS['domain_five']):
                self.add(land, f'enemy_land_{i}', self.enemy)
        if self.mode == 'domain_play':
            self.add('Tropical Island', 'new_dual', self.owner, 'hand')
        if self.mode == 'domain_remove':
            self.add('Stone Rain', 'land_removal', self.enemy, 'hand')

    def begin_scene(self):
        if self.mode == 'domain_play':
            self.emit('turn', player=self.owner, value=0)
            self.fund(self.active, [0] * 6)
            self.emit('play_land', ref='new_dual', value=1)
            self.move('new_dual', 'battlefield')
            self.emit('refresh')
            self.receipt()
            self.emit('advance_turn')
            self.emit('untap_step', player=self.active)
            self.emit('turn', player=self.active, value=0)
            return
        super().begin_scene()
        if self.mode == 'domain_remove':
            self.cast('land_removal', target='land_2')

    def combat(self, committed):
        super().combat(committed)
        if self.mode != 'vigilance_blocks_next':
            return
        self.emit('advance_turn')
        self.active, self.defender = self.enemy, self.owner
        self.emit('untap_step', player=self.active)
        self.emit('turn', player=self.active, value=0)
        self.observe('source', 'tapped', 0)
        self.first = 'enemy_attacker'
        self.paid_declaration()
        self.emit('begin_blockers')
        self.emit('block', ref='source', other='enemy_attacker', value=1)
        self.emit('advance_combat')
        self.observe('enemy_attacker', 'blocked', 1)
        self.emit('combat_damage')
        self.entities['source']['damage'] = self.entities['enemy_attacker']['stats'][0]
        self.move('enemy_attacker', 'graveyard')
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')
        self.emit('cleanup')
        self.entities['source']['damage'] = 0
        self.emit('refresh')
        self.receipt()



WITNESSES |= COMPOSITION_WITNESSES


def classify(record):
    try:
        return _pilot_parameters(record)
    except ValueError:
        try:
            return composition_parameters(record)
        except ValueError:
            return upkeep_parameters(record)


# Added only in the held upkeep composition preview.

from cumulative_upkeep_payment_recipes import REMINDER as CUMULATIVE_REMINDER


UPKEEP_ELEPHANT = ('pay_exact', 'underfunded', 'black', 'black_multicolor', 'walker_short', 'age_paid_once',
                   'age_paid_twice', 'age_decline', 'age_unfunded', 'cast_source')
UPKEEP_KOSKUN = ('pay_exact', 'underfunded', 'stack_distinct', 'walker_short', 'upkeep_tap', 'upkeep_decline',
                 'upkeep_empty', 'upkeep_enemy', 'upkeep_tapped', 'world_replaced', 'cast_source')
UPKEEP_WITNESSES = {'Walking Corpse', 'Golgari Guildmage', 'Colossus of Sardia', 'Concordant Crossroads'}
BLACK_MODES = {'black', 'black_multicolor'}
ELEPHANT_AFFORDABLE_UPKEEP = {'age_paid_once', 'age_paid_twice', 'age_decline'}
KOSKUN_CHOICES = {'upkeep_tap', 'upkeep_decline'}


def upkeep_parameters(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete normal attack-tax source required')
    lines = record['oracle_text'].splitlines()
    printed = record['mana_cost']
    if record['type_line'] == 'Enchantment' and record['keywords'] == ['Cumulative upkeep']:
        upkeep = re.fullmatch(r'Cumulative upkeep (\{[1-9]\})' + re.escape(CUMULATIVE_REMINDER), lines[0] if lines else '')
        tax = "Nonblack creatures can't attack you unless their controller pays {2} for each creature they control that's attacking you."
        if upkeep and lines == [upkeep[0], "Black creatures can't attack you.", tax] and printed == '{G}':
            return dict(cost=cost_from_oracle(record), composition='elephant', tax_formula='fixed', amount=2,
                        upkeep_per_age=int(upkeep[1][1]), protects_walker=False, flying=False)
    tapping = 'At the beginning of your upkeep, sacrifice this enchantment unless you tap an untapped creature you control.'
    if (record['type_line'] == 'World Enchantment' and record['keywords'] == []
            and re.fullmatch(r'\{[1-9]\}\{B\}\{B\}', printed) and lines == [tapping, FIXED % 2]):
        return dict(cost=cost_from_oracle(record), composition='koskun', tax_formula='fixed', amount=2,
                    protects_walker=False, flying=False)
    raise ValueError('complete reviewed upkeep plus attack-tax clauses required')


class UpkeepBoard(Board):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        super().__init__(name, csvid, declaration, params, variant, rows, oracle)
        self.age = 0
        mode = self.mode
        if mode in ELEPHANT_AFFORDABLE_UPKEEP:
            self.add('Forest', 'age_land', self.owner)
        if mode == 'age_paid_twice':
            self.add('Plains', 'second_age_land', self.owner, 'hand')
        if mode in KOSKUN_CHOICES:
            self.add('Grizzly Bears', 'upkeep_creature', self.owner)
        if mode == 'upkeep_tapped':
            self.add('Colossus of Sardia', 'upkeep_creature', self.owner, state='tapped')
        if mode == 'upkeep_enemy':
            self.add('Grizzly Bears', 'enemy_upkeep_creature', self.enemy)
            self.add('Darksteel Relic', 'own_noncreature', self.owner)
        if mode == 'world_replaced':
            self.add('Concordant Crossroads', 'new_world', self.enemy, 'hand')
        if mode == 'stack_distinct':
            self.add('Ghostly Prison', 'other_source', self.owner)

    def receipt(self):
        super().receipt()
        if self.params['composition'] == 'elephant' and self.entities['source']['zone'] == 'battlefield':
            self.emit('assert_counter', ref='source', counter_kind='age', value=self.age)

    def begin_scene(self):
        from recipes import COLORS, cost_from_oracle

        super().begin_scene()
        mode = self.mode
        if mode in BLACK_MODES:
            runner = self.entities['first']['name']
            self.emit('assert_matching_permanents', player=self.active, index=self.rows[runner]['csvid'], property='color',
                      value=sum(1 << COLORS[color] for color in self.oracle[runner]['colors']))
        if mode == 'world_replaced':
            fee = cost_from_oracle(self.oracle['Concordant Crossroads'])
            self.fund(self.active, fee)
            self.emit('cast', ref='new_world', value=1)
            self.emit('assert_stack', value=1)
            self.mana[self.active] = [0] * 6
            self.pools()
            self.emit('resolve')
            self.move('new_world', 'battlefield')
            self.move('source', 'graveyard')
            self.emit('refresh')
            self.receipt()
        if mode in ELEPHANT_AFFORDABLE_UPKEEP | {'age_unfunded'}:
            self.visit_for_upkeep(self.owner)
            self.paid_age(['age_land'] if mode != 'age_unfunded' else [], decline=mode == 'age_decline')
            if mode == 'age_paid_twice':
                self.emit('turn', player=self.owner, value=0)
                self.emit('play_land', ref='second_age_land', value=1)
                self.move('second_age_land', 'battlefield')
                self.visit_for_upkeep(self.enemy)
                self.visit_for_upkeep(self.owner)
                self.paid_age(['age_land', 'second_age_land'])
            self.finish_visit()
        if mode in KOSKUN_CHOICES | {'upkeep_empty', 'upkeep_enemy', 'upkeep_tapped'}:
            self.visit_for_upkeep(self.owner)
            if mode == 'upkeep_tap':
                self.emit('choose_target', player=self.owner, ref='upkeep_creature')
            if mode == 'upkeep_decline':
                self.emit('choose_optional_target', player=self.owner, ref='upkeep_creature')
            self.emit('upkeep_step', player=self.owner)
            if mode == 'upkeep_decline':
                self.emit('assert_forced_target_choices', value=0)
            self.emit('assert_unused_choices', value=0)
            if mode == 'upkeep_tap':
                self.entities['upkeep_creature']['tapped'] = True
            else:
                self.move('source', 'graveyard')
            self.receipt()
            self.finish_visit()

    def visit_for_upkeep(self, player):
        assert not any(map(any, self.mana))
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        for entity in self.entities.values():
            if entity['zone'] == 'battlefield' and entity['player'] == player:
                if "doesn't untap during your untap step" not in self.oracle[entity['name']]['oracle_text']:
                    entity['tapped'] = False

    def paid_age(self, available_lands, *, decline=False):
        self.age += 1
        amount = self.age * self.params['upkeep_per_age']
        can_pay = len(available_lands) >= amount
        if can_pay or self.owner == 1:
            self.emit('choose_dialog', player=self.owner, value=int(decline or not can_pay))
        if can_pay and not decline:
            for ref in available_lands[:amount]:
                if self.owner == 0:
                    self.emit('choose_mana_source', ref=ref)
                self.entities[ref]['tapped'] = True
        else:
            self.move('source', 'graveyard')
        self.emit('upkeep_step', player=self.owner)
        self.emit('assert_unused_choices', value=0)
        self.receipt()

    def finish_visit(self):
        self.visit_for_upkeep(self.active)
        self.emit('turn', player=self.active, value=0)


WITNESSES |= UPKEEP_WITNESSES
