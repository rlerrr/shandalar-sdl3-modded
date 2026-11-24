"""Whole-Oracle spells and effects that force all eligible creatures to block."""

import json
import re
from collections import Counter
from pathlib import Path

from cast_untap_trigger_recipes import exact


FAMILY = 'must_block_creature_spell'
PROFILE = Path(__file__).parent / 'data/must_block_creature_spell_wave8_profiles.json'
TARGET = 'All creatures able to block target creature this turn do so.'
STATIC = 'All creatures able to block this creature do so.'
SELF = 'All creatures able to block this creature this turn do so.'
EQUIP = 'All creatures able to block equipped creature do so.'
FEROCIOUS = 'Ferocious — That creature gains indestructible until end of turn if you control a creature with power 4 or greater.'
LIBRARY = ('Forest', 'Mountain', 'Plains', 'Grizzly Bears')
WITNESSES = set(LIBRARY) | {'Hill Giant', 'Maritime Guard', 'Carrion Feeder', 'Suntail Hawk', 'Giant Spider',
                           'Darksteel Relic', 'Deadly Insect', 'Rumbling Baloth'}
COMMON = ('force_one', 'force_two', 'decline', 'competing', 'all_tapped', 'illegal_blocker', 'idle_target', 'after_cleanup')
CAST_TARGET = ('enemy_target', 'no_target', 'shroud_target', 'unfunded', 'target_lost', 'flyer_reach')
FEROCIOUS_MODES = ('power_three', 'power_four', 'enemy_power_four', 'support_lost', 'support_after', 'destroy_after_expiry')
EQUIP_MODES = ('unattached', 'wrong_controller', 'unfunded_equip', 'during_combat', 'cast_equip', 'retarget_equip', 'removed_equip')
CHANNEL_MODES = ('channel', 'channel_unfunded', 'channel_no_target', 'channel_defending')
NO_COMBAT = {'no_target', 'shroud_target', 'wrong_controller', 'channel_no_target'}
OFF_TURN = {'instant_defending', 'channel_defending'}
DEFERRED = OFF_TURN | {'instant_after_attack', 'activate_after_attack'}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def _pilot_parameters(record):
    from recipes import cost_from_oracle

    def cost(text):
        return cost_from_oracle(dict(mana_cost=text))

    printed = record.get('mana_cost', '')
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not re.fullmatch(r'\{[1-9]\}(?:\{[BG]\}){0,2}', printed)):
        raise ValueError('fixed-cost normal source required')
    lines = record.get('oracle_text', '').splitlines()
    kind, keywords = record.get('type_line', ''), record.get('keywords')
    params = dict(cost=cost(printed))
    if kind in ('Instant', 'Sorcery') and lines in ([TARGET], [TARGET, FEROCIOUS]):
        threshold = 4 if lines[-1] == FEROCIOUS else 0
        if keywords == (['Ferocious'] if threshold else []):
            return dict(params, mechanism='spell', instant=kind == 'Instant', ferocious=threshold)
    if kind == 'Artifact — Equipment' and len(lines) == 2 and lines[0] == EQUIP and keywords == ['Equip']:
        match = re.fullmatch(r'Equip (\{[1-9]\}) \(\1: Attach to target creature you control\. Equip only as a sorcery\.\)', lines[1])
        if match:
            return dict(params, mechanism='equip', fee=cost(match[1]))
    if (kind.startswith('Creature — ') and all(re.fullmatch(r'[1-9]', record.get(k, '')) for k in ('power', 'toughness'))):
        if len(lines) == 1 and not keywords:
            match = re.fullmatch(r'(\{[1-9]\}\{G\}): ' + re.escape(SELF), lines[0])
            if match:
                return dict(params, mechanism='self', fee=cost(match[1]))
        if len(lines) == 2 and lines[0] == STATIC and keywords == ['Channel']:
            match = re.fullmatch(r'Channel — (\{[1-9]\}\{G\}\{G\}), Discard this card: ' + re.escape(TARGET), lines[1])
            if match:
                return dict(params, mechanism='channel', fee=cost(match[1]))
    raise ValueError('complete reviewed forced-block mechanism required')


def variants(params):
    if 'composition' in params:
        return [f'{mode}_{seat}' for seat in (0, 1) for mode in COMPOSITION_MODES[params['composition']]]
    kind = params['mechanism']
    modes = list(COMMON)
    if kind == 'spell':
        modes.extend(CAST_TARGET)
        if params['instant']:
            modes.extend(('instant_after_attack', 'instant_defending'))
        if params['ferocious']:
            modes.extend(FEROCIOUS_MODES)
    elif kind == 'equip':
        modes.extend(EQUIP_MODES)
    elif kind == 'self':
        modes.extend(('unfunded_activate', 'activate_after_attack', 'cast_then_force'))
    else:
        modes.extend(CHANNEL_MODES + ('cast_then_force',))
    return [f'{mode}_{seat}' for seat in (0, 1) for mode in modes]


def spec(declaration, record):
    params = classify(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(profile()['sources'].get(record['name']), record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(params, declaration.get('params'))):
        raise ValueError('exact whole Oracle and typed forced-block recipe required')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v, str) for v in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(params))):
        raise ValueError('distinct reviewed forced-block variants required')
    return dict(params, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profile()
    records = data['sources'] | data['witnesses']
    for primary, face in data['engine_faces'].items():
        orig = rows[primary]
        if (primary not in data['sources'] or not exact(rows.get(face['name']), face)
                or face['csvid'] != orig['csvid'] + 1 or face['handler'] != orig['handler']):
            raise ValueError(f'paired production Bestow identity changed for {primary}')
    if set(data['witnesses']) != WITNESSES or set(data['registry']) != set(records):
        raise ValueError('pinned complete source and witness inventory required')
    for name, record in records.items():
        row = rows[name]
        if (not exact(oracle[name], record) or row['kind'] != 'card'
                or not exact(data['registry'][name], {k: row[k] for k in ('csvid', 'handler')})):
            raise ValueError(f'Oracle or registry drift for {name}')
        if name in data['sources'].keys() | {'Carrion Feeder', 'Fog', 'Darksteel Myr'} and row['handler'] in ('card_dummy', 'NULL', '0'):
            raise ValueError(f'implemented source or rules witness required for {name}')


class Board:
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active = self.enemy if self.mode in OFF_TURN else self.owner
        self.defender = 1 - self.active
        self.name, self.csvid, self.variant = name, csvid, variant
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.kind = params['mechanism']
        self.channel = self.mode in CHANNEL_MODES
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.indestructible = set()
        mode = self.mode
        hand = self.kind == 'spell' or self.channel or mode in ('cast_then_force', 'cast_equip')
        self.add(name, 'source', self.owner, 'hand' if hand else 'battlefield')
        self.carrier = 'source' if self.kind in ('self', 'channel') and not self.channel else 'carrier'
        if mode not in NO_COMBAT and self.carrier != 'source':
            self.add('Suntail Hawk' if mode == 'flyer_reach' else 'Hill Giant', 'carrier', self.active)
        if mode in ('competing', 'idle_target', 'retarget_equip'):
            self.add('Grizzly Bears', 'ally', self.active)
        if mode == 'enemy_target':
            self.add('Maritime Guard', 'enemy_target', self.enemy, tapped=True)
        if mode == 'shroud_target':
            self.add('Deadly Insect', 'shrouded', self.owner)
        if mode in ('no_target', 'channel_no_target', 'wrong_controller'):
            self.add('Darksteel Relic', 'artifact', self.owner)
        if mode == 'wrong_controller':
            self.add('Hill Giant', 'enemy_target', self.enemy)
        if mode not in NO_COMBAT:
            blocker = 'Hill Giant' if mode in FEROCIOUS_MODES else 'Carrion Feeder' if mode == 'illegal_blocker' else 'Grizzly Bears'
            self.add(blocker, 'blocker', self.defender, tapped=mode == 'all_tapped')
        if mode in ('force_two', 'flyer_reach'):
            self.add('Giant Spider' if mode == 'flyer_reach' else 'Maritime Guard', 'blocker_two', self.defender)
        if mode in FEROCIOUS_MODES:
            witness = 'Hill Giant' if mode == 'power_three' else 'Rumbling Baloth'
            self.add(witness, 'support', self.enemy if mode == 'enemy_power_four' else self.owner, tapped=True)
        for who in (0, 1):
            self.add('Forest', f'grave_{who}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            for index, card in enumerate(LIBRARY):
                self.add(card, f'lib_{who}_{index}', who, 'library')

    def add(self, name, ref, player, zone='battlefield', *, tapped=False):
        from recipes import card

        item = card(name, ref, player, zone)
        if zone == 'battlefield':
            item['state'] = ['tapped' if tapped else 'untapped']
        if zone in ('hand', 'battlefield'):
            record = self.oracle[name]
            stats = tuple(int(record[k]) for k in ('power', 'toughness')) if 'power' in record else None
            self.entities[ref] = dict(name=name, player=player, zone=zone, stats=stats, tapped=tapped, damage=0)
        self.setup.append(item)
        self.zones[zone][player].append(name)

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op, **fields))

    def observe(self, ref, field, value):
        self.emit('assert_card', ref=ref, property=field, value=int(value))

    def move(self, ref, destination):
        entity = self.entities[ref]
        name, owner, zone = (entity[key] for key in ('name', 'player', 'zone'))
        self.zones[zone][owner].remove(name)
        self.zones[destination][owner].append(name)
        entity['zone'] = destination

    def fund(self, amount):
        self.mana[self.owner] = list(amount)
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('set_mana', player=who, color=color, value=value)
        self.emit('refresh')

    def payment(self, amount):
        self.mana[self.owner] = [have - spent for have, spent in zip(self.mana[self.owner], amount)]
        for color, value in enumerate(self.mana[self.owner]):
            self.emit('assert_mana', player=self.owner, color=color, value=value)

    def receipt(self, *, variable_defender=False):
        self.emit('assert_stack', value=0)
        self.emit('assert_choices_consumed')
        for ref, entity in self.entities.items():
            if entity['zone'] != 'battlefield' or variable_defender and entity['player'] == self.defender:
                continue
            self.observe(ref, 'in_play', 1)
            self.observe(ref, 'tapped', entity['tapped'])
            if entity['stats'] is not None:
                for key, value in zip(('power', 'toughness', 'damage'), (*entity['stats'], entity['damage'])):
                    self.observe(ref, key, value)
        names = sorted({entity['name'] for entity in self.entities.values()})
        for who in (0, 1):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, value in enumerate(self.mana[who]):
                self.emit('assert_mana', player=who, color=color, value=value)
            for zone, per_player in self.zones.items():
                if variable_defender and who == self.defender and zone in ('battlefield', 'graveyard'):
                    continue
                contents = per_player[who]
                self.emit('assert_zone', player=who, zone=zone, value=len(contents))
                if zone == 'library':
                    for index, card in enumerate(contents):
                        self.emit('assert_zone_card', player=who, zone=zone, index=index, value=self.rows[card]['csvid'])
                else:
                    counts = Counter(contents)
                    for name in names:
                        self.emit('assert_zone_multiplicity', player=who, zone=zone, index=self.rows[name]['csvid'], value=counts[name])

    def destroy(self, ref):
        self.emit('destroy', ref=ref)
        self.move(ref, 'graveyard')
        self.emit('refresh')

    def cast_permanent(self):
        self.emit('cast', ref='source', value=1)
        self.emit('require_card', ref='source', property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.payment(self.params['cost'])
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.emit('refresh')
        self.receipt()

    def next_own_turn(self):
        for player in (self.enemy, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=player)
            for entity in self.entities.values():
                if entity['player'] == player and entity['zone'] == 'battlefield':
                    entity['tapped'] = False
        self.indestructible.clear()
        self.emit('turn', player=self.owner, value=0)

    def cast_lure(self, target):
        mode = self.mode
        if mode in ('no_target', 'shroud_target'):
            self.emit('can_cast', ref='source', value=0)
            self.emit('cast', ref='source', value=0)
            return None
        if mode == 'unfunded':
            self.emit('cast', ref='source', value=0)
            return None
        self.emit('choose_target', ref=target, player=self.owner)
        self.emit('cast', ref='source', value=1)
        self.emit('require_card', ref='source', property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.payment(self.params['cost'])
        if mode == 'target_lost':
            self.destroy(target)
        if mode == 'support_lost':
            self.destroy('support')
        threshold = self.params['ferocious']
        if threshold and any(ent['zone'] == 'battlefield' and ent['player'] == self.owner and ent['stats'][0] >= threshold
                             for ent in self.entities.values() if ent['stats']):
            self.indestructible.add(target)
        self.emit('resolve')
        self.move('source', 'graveyard')
        self.emit('refresh')
        if mode == 'support_after':
            self.destroy('support')
        self.receipt()
        return target if mode != 'target_lost' else None

    def activate_lure(self, target):
        mode = self.mode
        if self.channel:
            if mode in ('channel_unfunded', 'channel_no_target'):
                self.emit('can_activate_hand', ref='source', value=0)
                self.emit('activate_hand', ref='source', value=0)
                return None
            self.emit('require_can_activate_hand', ref='source', value=1)
            self.emit('choose_dialog', player=self.owner, value=1)
            self.emit('choose_target', ref=target, player=self.owner)
            self.emit('activate_hand', ref='source', value=1)
            self.observe('source', 'in_hand', 0)
            self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard', index=self.csvid, value=1)
            self.move('source', 'graveyard')
        else:
            if mode in ('unfunded_activate', 'unfunded_equip', 'wrong_controller', 'during_combat'):
                self.emit('can_activate', ref='source', value=0)
                self.emit('activate', ref='source', value=0)
                return None
            self.emit('require_can_activate', ref='source', value=1)
            if self.kind == 'equip':
                self.emit('choose_target', ref=target, player=self.owner)
            self.emit('activate', ref='source', value=1)
        self.emit('assert_stack', value=1)
        self.payment(self.params['fee'])
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()
        return target

    def apply(self):
        mode = self.mode
        target = 'enemy_target' if mode == 'enemy_target' else self.carrier
        if self.kind == 'spell':
            return self.cast_lure(target)
        if self.kind == 'channel' and not self.channel:
            return self.carrier
        if mode == 'unattached':
            return None
        target = self.activate_lure(target)
        if mode == 'retarget_equip':
            return self.activate_lure('ally')
        if mode == 'removed_equip':
            self.destroy('source')
            return None
        return target

    def attack(self):
        refs = [] if self.entities[self.carrier]['zone'] != 'battlefield' else [self.carrier]
        if self.mode == 'idle_target':
            refs = ['ally']
        elif self.mode in ('competing', 'retarget_equip'):
            refs.append('ally')
        if not refs:
            return []
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref=ref, value=1)
            self.entities[ref]['tapped'] = True
            self.observe(ref, 'attacking', 1)
        self.emit('finish_attackers')
        return refs

    def combat(self, attackers, lure):
        mode = self.mode
        forced = []
        if lure in attackers and mode not in ('all_tapped', 'illegal_blocker'):
            forced = ['blocker_two'] if mode == 'flyer_reach' else ['blocker', 'blocker_two'] if mode == 'force_two' else ['blocker']
        if forced and mode == 'decline':
            self.emit('choose_optional_target', ref=forced[0], player=self.defender)
        for blocker in forced:
            self.emit('choose_target', ref=blocker, player=self.defender)
        self.emit('begin_blockers')
        if forced:
            self.emit('assert_unused_choices', value=0)
        else:
            self.emit('assert_choices_consumed')
        if (mode in ('competing', 'retarget_equip') and forced):
            other = next(ref for ref in attackers if ref != lure)
            self.emit('block', ref='blocker', other=other, value=0)
        if mode in ('all_tapped', 'illegal_blocker', 'flyer_reach'):
            self.emit('block', ref='blocker', other=attackers[0], value=0)
        if mode in ('idle_target', 'enemy_target'):
            self.emit('block', ref='blocker', other=attackers[0], value=1)
            forced, lure = ['blocker'], attackers[0]
        self.emit('advance_combat')
        for ref in attackers:
            self.observe(ref, 'blocked', bool(forced) and ref == lure)
        for ref in ('blocker', 'blocker_two'):
            if ref in self.entities:
                self.observe(ref, 'blocking_count', ref in forced)
        self.receipt()
        if len(forced) > 1 and self.active == 0:
            power = self.entities[lure]['stats'][0]
            for _ in range(power):
                self.emit('choose_target', ref=forced[0], player=0)
        self.emit('combat_damage')
        for ref in attackers:
            attacker = self.entities[ref]
            if ref != lure or not forced:
                self.life[self.defender] -= attacker['stats'][0]
                continue
            attacker['damage'] += sum(self.entities[blocker]['stats'][0] for blocker in forced)
            if len(forced) == 1 or self.active == 0:
                self.entities[forced[0]]['damage'] += attacker['stats'][0]
        for ref, ent in self.entities.items():
            if ent['zone'] == 'battlefield' and ent['stats'] and ent['damage'] >= ent['stats'][1] and ref not in self.indestructible:
                self.move(ref, 'graveyard')
        self.emit('resolve')
        self.emit('refresh')
        variable = len(forced) > 1 and self.active == 1
        self.receipt(variable_defender=variable)
        self.emit('end_combat')
        self.emit('cleanup')
        self.indestructible.clear()
        for ent in self.entities.values():
            ent['damage'] = 0
        self.emit('refresh')
        if mode == 'destroy_after_expiry':
            self.destroy(self.carrier)
        self.receipt(variable_defender=variable)

    def case(self):
        from recipes import step

        mode = self.mode
        self.emit('turn', player=self.active, value=0)
        zero = [0] * 6
        if self.kind == 'spell':
            fund = zero if mode == 'unfunded' else self.params['cost']
        elif mode == 'cast_equip':
            fund = [cast + equip for cast, equip in zip(self.params['cost'], self.params['fee'])]
        elif mode == 'cast_then_force':
            fund = self.params['cost']
        elif mode in ('unfunded_equip', 'unfunded_activate', 'channel_unfunded', 'unattached') or (self.kind == 'channel' and not self.channel):
            fund = zero
        else:
            fund = [amount * (2 if mode == 'retarget_equip' else 1) for amount in self.params['fee']]
        self.fund(fund)
        cut = len(self.steps)
        self.receipt()
        if mode in ('cast_then_force', 'cast_equip'):
            self.cast_permanent()
            if mode == 'cast_then_force':
                self.next_own_turn()
                self.observe('source', 'sick', 0)
            if self.kind == 'self':
                self.fund(self.params['fee'])
        acting_late = mode in DEFERRED or mode == 'during_combat'
        attackers = self.attack() if acting_late else []
        lure = self.apply()
        if mode == 'after_cleanup':
            self.next_own_turn()
            if self.kind in ('self', 'spell'):
                lure = None
        if mode not in NO_COMBAT:
            if not acting_late:
                attackers = self.attack()
            if attackers:
                self.combat(attackers, lure)
        else:
            self.receipt()
        suffix = self.steps[cut:]
        run = self.steps[:cut] + [step('capture')] + suffix
        run += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'MustBlockCreatureSpellRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=run, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'One bounded combat and real cleanup, without banding, attack taxes or competing mandatory requirements.',
                            'AI multi-block outgoing distribution is a legal choice; blocked identities, incoming damage and player damage remain independently checked.',
                        ]))


def expand(name, csvid, declaration, params, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    scene = CompositionBoard if 'composition' in params else Board
    return [scene(name, csvid, declaration, params, variant, rows, oracle).case() for variant in selected]


# Private extension appended to the staged family module by stage_followers.py.

from bestow_stat_aura_recipes import REMINDER as BESTOW_REMINDER


COMPOSITION_MODES = {
    'threshold_aura': ('threshold_six', 'threshold_seven', 'enemy_seven', 'foreign_host',
                       'foreign_enemy_seven', 'aura_removed', 'no_host', 'unfunded'),
    'threshold_basilisk': ('six_unforced', 'six_manual', 'seven_forced', 'enemy_only_seven',
                           'cast_then_force', 'indestructible', 'prevented', 'defending_damage'),
    'color_aura': ('black_paid_b', 'green_paid_g', 'both_paid_b', 'neither_paid_g',
                   'foreign_host', 'aura_removed', 'wrong_color', 'no_host'),
    'bestow_lure': ('ordinary', 'bestow', 'bestow_foreign', 'bestow_persistent', 'detach',
                    'pending_lost', 'unfunded', 'no_host_regular', 'cancel_mode', 'aura_removed'),
}
COMPOSITION_WITNESSES = {'Wall of Stone', 'Darksteel Myr', 'Siege Mastodon', 'Walking Corpse',
                         'Golgari Guildmage', 'Fog'}
THRESHOLD_TEXT = 'Threshold — As long as there are seven or more cards in your graveyard, all creatures able to block {} do so.'
DELAY = 'Whenever this creature deals combat damage to a creature, destroy that creature at end of combat.'
COLOR_BLACK = 'As long as enchanted creature is black, it gets +1/+1 and has deathtouch. (Any amount of damage it deals to a creature is enough to destroy that creature.)'
COLOR_GREEN = 'As long as enchanted creature is green, it gets +1/+1 and all creatures able to block it do so.'


def composition_parameters(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('complete normal source required')
    lines = record['oracle_text'].splitlines()
    ctype, tags = record['type_line'], record['keywords']
    fixed_cost = re.fullmatch(r'\{[1-9]\}(?:\{G\}){1,3}', record['mana_cost'])
    if ctype == 'Enchantment — Aura':
        if fixed_cost and tags == ['Enchant', 'Threshold']:
            match = re.fullmatch(r'Enchanted creature gets \+([1-4])/\+([1-4])\.', lines[1] if len(lines) > 1 else '')
            if match and lines == ['Enchant creature', match[0], THRESHOLD_TEXT.format('enchanted creature')]:
                return dict(composition='threshold_aura', threshold=7,
                            cost=cost_from_oracle(record), bonus=list(map(int, match.groups())))
        hybrid = re.fullmatch(r'(\{[1-9]\})\{B/G\}', record['mana_cost'])
        if hybrid and tags == ['Enchant'] and lines == ['Enchant creature', COLOR_BLACK, COLOR_GREEN]:
            return dict(composition='color_aura', bonus=[1, 1], fees={color: cost_from_oracle({'mana_cost': hybrid[1] + '{' + color + '}'})
                                                                    for color in ('B', 'G')})
    if fixed_cost and tags == ['Threshold'] and ctype.startswith('Creature — ') and lines == [DELAY, THRESHOLD_TEXT.format('this creature')]:
        return dict(composition='threshold_basilisk', threshold=7, cost=cost_from_oracle(record))
    if fixed_cost and tags == ['Bestow'] and ctype.startswith('Enchantment Creature — ') and len(lines) == 3:
        price = re.fullmatch(r'Bestow (\{[1-9]\}\{G\})', lines[0].removesuffix(BESTOW_REMINDER))
        bonus = re.fullmatch(r'Enchanted creature gets \+([1-4])/\+([1-4])\.', lines[-1])
        if price and bonus and lines[0].endswith(BESTOW_REMINDER) and lines[1] == 'All creatures able to block this creature or enchanted creature do so.':
            return dict(composition='bestow_lure', cost=cost_from_oracle(record),
                        bestow=cost_from_oracle({'mana_cost': price[1]}), bonus=list(map(int, bonus.groups())))
    raise ValueError('complete reviewed forced-block mechanism required')


class CompositionBoard(Board):
    def __init__(self, name, csvid, declaration, params, variant, rows, oracle):
        self.mode, seat = variant.rsplit('_', 1)
        self.owner, self.enemy = int(seat), 1 - int(seat)
        self.active, self.defender = self.owner, self.enemy
        self.name, self.csvid, self.variant = name, csvid, variant
        self.params, self.declaration, self.rows, self.oracle = params, declaration, rows, oracle
        self.composition = params['composition']
        self.setup, self.steps, self.entities = [], [], {}
        self.zones = {zone: [[], []] for zone in ('hand', 'battlefield', 'graveyard', 'library', 'exile')}
        self.mana, self.life = [[0] * 6, [0] * 6], [20, 20]
        self.indestructible = set()
        self.host = None
        self.attachment = False
        self.form = None
        self.bonus = [0, 0]
        mode = self.mode
        self.foreign_host = mode in ('foreign_host', 'foreign_enemy_seven', 'bestow_foreign')
        basilisk = self.composition == 'threshold_basilisk'
        cast_source = not basilisk or mode == 'cast_then_force'
        self.add(name, 'source', self.owner, 'hand' if cast_source else 'battlefield')
        if not basilisk and mode not in ('no_host', 'no_host_regular'):
            colors = {'black_paid_b': 'Walking Corpse', 'green_paid_g': 'Grizzly Bears', 'both_paid_b': 'Golgari Guildmage',
                      'neither_paid_g': 'Maritime Guard', 'foreign_host': 'Grizzly Bears'}
            host = colors.get(mode, 'Grizzly Bears') if self.composition == 'color_aura' else 'Hill Giant'
            self.add(host, 'host', self.enemy if self.foreign_host else self.owner)
            self.host = 'host'
        if mode in ('no_host', 'no_host_regular'):
            self.add('Darksteel Relic', 'noncreature', self.owner)
        elif mode == 'defending_damage':
            self.add('Siege Mastodon', 'other_attacker', self.enemy)
        elif basilisk or mode not in ('unfunded', 'wrong_color', 'cancel_mode'):
            blocker = 'Darksteel Myr' if mode == 'indestructible' else 'Wall of Stone'
            self.add(blocker, 'blocker', self.owner if self.foreign_host else self.enemy)
        if mode == 'prevented':
            self.add('Fog', 'fog', self.owner, 'hand')
        for who in (0, 1):
            size = self.grave_size(who)
            for index in range(size):
                self.add('Forest', f'grave_{who}_{index}', who, 'graveyard')
            self.add('Plains', f'exile_{who}', who, 'exile')
            for index, card in enumerate(LIBRARY):
                self.add(card, f'lib_{who}_{index}', who, 'library')

    def grave_size(self, who):
        if 'threshold' not in self.params:
            return 1
        ours = who == self.owner
        threshold = self.params['threshold']
        if self.composition == 'threshold_aura':
            own_high = self.mode not in ('threshold_six', 'enemy_seven', 'foreign_enemy_seven')
            enemy_high = self.mode in ('enemy_seven', 'foreign_enemy_seven')
        else:
            own_high = self.mode not in ('six_unforced', 'six_manual', 'enemy_only_seven')
            enemy_high = self.mode == 'enemy_only_seven'
        return threshold if (own_high if ours else enemy_high) else threshold - 1

    def card_form(self, bestowed):
        source = self.entities['source']
        form = profile()['engine_faces'][self.name]['name'] if bestowed else self.name
        whose = self.zones['battlefield'][self.owner]
        whose[whose.index(source['name'])] = form
        source['name'] = form
        source['stats'] = None if bestowed else tuple(int(self.oracle[self.name][part]) for part in ('power', 'toughness'))
        self.observe('source', 'csvid', self.rows[form]['csvid'])
        self.form = 'aura' if bestowed else 'creature'

    def cast_composition(self):
        mode = self.mode
        bestow = self.composition == 'bestow_lure'
        is_aura = self.composition in ('color_aura', 'threshold_aura')
        bestowed = bestow and mode not in ('ordinary', 'no_host_regular', 'unfunded', 'cancel_mode')
        funded = mode not in ('unfunded', 'wrong_color')
        if self.composition == 'color_aura':
            fee = self.params['fees']['B' if mode in ('black_paid_b', 'both_paid_b') else 'G']
        else:
            fee = self.params['bestow'] if bestowed or mode == 'cancel_mode' else self.params['cost']
        if not funded or is_aura and self.host is None:
            if self.host is None:
                self.emit('can_cast', ref='source', value=0)
            self.emit('cast', ref='source', value=0)
            self.observe('source', 'in_hand', 1)
            self.receipt()
            return False
        if bestow and (bestowed or mode == 'cancel_mode'):
            self.emit('present_ai_dialogs', value=1)
            self.emit('choose_dialog', player=self.owner, value=2 if mode == 'cancel_mode' else 0)
        if mode == 'cancel_mode':
            self.emit('cast', ref='source', value=0)
            self.observe('source', 'in_hand', 1)
            self.receipt()
            return False
        if is_aura or bestowed:
            self.emit('choose_target', ref='host', player=self.owner)
        self.emit('cast', ref='source', value=1)
        self.emit('require_card', ref='source', property='in_hand', value=0)
        self.emit('assert_stack', value=1)
        self.payment(fee)
        if bestow:
            pending = profile()['engine_faces'][self.name]['csvid'] if bestowed else self.csvid
            self.observe('source', 'csvid', pending)
        if mode == 'pending_lost':
            self.destroy('host')
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.attachment = is_aura or (bestowed and mode != 'pending_lost')
        self.emit('refresh')
        if bestow:
            self.card_form(self.attachment)
        if self.attachment:
            self.update_host_stats()
        self.receipt()
        return True

    def update_host_stats(self):
        host = self.entities['host']
        record = self.oracle[host['name']]
        relevant = len(set(record['colors']) & {'B', 'G'}) if self.composition == 'color_aura' else 1
        bonus = relevant if self.attachment else 0
        host['stats'] = tuple(int(record[key]) + bonus * extra for key, extra in zip(('power', 'toughness'), self.params['bonus']))
        if self.composition == 'color_aura':
            from recipes import COLORS

            self.emit('assert_matching_permanents', player=host['player'], index=self.rows[host['name']]['csvid'],
                      property='color', value=sum(1 << COLORS[c] for c in record['colors']))

    def forced(self, attacker):
        if self.composition == 'color_aura':
            return self.attachment and 'G' in self.oracle[self.entities[attacker]['name']]['colors']
        if self.composition == 'bestow_lure':
            return self.attachment or attacker == 'source'
        return (self.composition == 'threshold_basilisk' or self.attachment) and len(self.zones['graveyard'][self.owner]) >= self.params['threshold']

    def combat_one(self, attacker, blocker, *, force=False, optional=False, fog=False):
        from recipes import cost_from_oracle

        self.emit('begin_attackers')
        self.emit('attack', ref=attacker, value=1)
        self.entities[attacker]['tapped'] = True
        self.observe(attacker, 'attacking', 1)
        self.emit('finish_attackers')
        if force:
            self.emit('choose_target', ref=blocker, player=self.defender)
        self.emit('begin_blockers')
        if force:
            self.emit('assert_unused_choices', value=0)
        if optional:
            self.emit('block', ref=blocker, other=attacker, value=1)
        self.emit('advance_combat')
        blocked = force or optional
        self.observe(attacker, 'blocked', blocked)
        self.observe(blocker, 'blocking_count', blocked)
        self.receipt()
        if fog:
            self.emit('cast', ref='fog', value=1)
            self.emit('assert_stack', value=1)
            self.payment(cost_from_oracle(self.oracle['Fog']))
            self.emit('resolve')
            self.move('fog', 'graveyard')
            self.receipt()
        self.emit('combat_damage')
        att, blk = self.entities[attacker], self.entities[blocker]
        delayed = None
        if not fog and blocked:
            att['damage'], blk['damage'] = blk['stats'][0], att['stats'][0]
            deathtouch = self.composition == 'color_aura' and self.attachment and 'B' in self.oracle[att['name']]['colors']
            if att['damage'] >= att['stats'][1]:
                self.move(attacker, 'graveyard')
            if (blk['damage'] >= blk['stats'][1] or (blk['damage'] > 0 and deathtouch)) and blocker not in self.indestructible:
                self.move(blocker, 'graveyard')
            if self.composition == 'threshold_basilisk':
                victim = blocker if attacker == 'source' else attacker
                if self.entities[victim]['zone'] == 'battlefield' and victim not in self.indestructible:
                    delayed = victim
        elif not blocked:
            self.life[self.defender] -= att['stats'][0]
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')
        self.emit('resolve')
        self.emit('refresh')
        if delayed:
            self.move(delayed, 'graveyard')
        self.receipt()
        self.emit('cleanup')
        for ent in self.entities.values():
            ent['damage'] = 0
        self.emit('refresh')
        self.receipt()

    def case(self):
        from recipes import cost_from_oracle, step

        mode, kind = self.mode, self.composition
        self.emit('turn', player=self.owner, value=0)
        zero = [0] * 6
        if mode == 'wrong_color':
            budget = [4, 0, 1, 0, 0, 0]
        elif mode == 'unfunded':
            budget = zero
        elif kind == 'color_aura':
            budget = self.params['fees']['B' if mode in ('black_paid_b', 'both_paid_b') else 'G']
        elif kind == 'bestow_lure' and mode not in ('ordinary', 'no_host_regular'):
            budget = self.params['bestow']
        elif mode == 'prevented':
            budget = cost_from_oracle(self.oracle['Fog'])
        elif kind == 'threshold_basilisk' and mode != 'cast_then_force':
            budget = zero
        else:
            budget = self.params['cost']
        self.fund(budget)
        cut = len(self.steps)
        self.receipt()
        if kind == 'threshold_basilisk':
            if mode == 'cast_then_force':
                self.cast_permanent()
                self.next_own_turn()
            if mode == 'defending_damage':
                self.switch_turn()
                self.combat_one('other_attacker', 'source', optional=True)
            else:
                if mode == 'indestructible':
                    self.indestructible.add('blocker')
                self.combat_one('source', 'blocker', force=self.forced('source'), optional=mode == 'six_manual', fog=mode == 'prevented')
        elif self.cast_composition():
            if self.foreign_host:
                self.switch_turn()
            if kind == 'bestow_lure' and mode in ('ordinary', 'pending_lost', 'detach', 'bestow_persistent'):
                self.next_own_turn()
            if mode == 'detach':
                self.destroy('host')
                self.card_form(False)
                self.attachment = False
                self.receipt()
            if mode == 'aura_removed':
                self.destroy('source')
                if kind == 'bestow_lure':
                    self.zones['graveyard'][self.owner].remove(self.entities['source']['name'])
                    self.zones['graveyard'][self.owner].append(self.name)
                    self.entities['source']['name'] = self.name
                self.attachment = False
                self.update_host_stats()
                self.receipt()
            if mode != 'no_host_regular':
                attacker = 'source' if kind == 'bestow_lure' and self.form == 'creature' else 'host'
                optional = mode == 'black_paid_b'
                self.combat_one(attacker, 'blocker', force=self.forced(attacker), optional=optional)
        suffix = self.steps[cut:]
        run = self.steps[:cut] + [step('capture')] + suffix
        run += [step('record_state'), step('restore')] + suffix + [step('assert_recorded_state')]
        return dict(name=f'MustBlockCreatureSpellRecipe_{self.csvid}_{self.variant}', card=self.name, fixture='empty_main', seed=123,
                    setup=self.setup, steps=run, _recipe=dict(family=FAMILY, version=2, variant=self.variant,
                        params=self.declaration['params'], oracle_text=self.declaration['oracle_text'], untested_components=[
                            'Uses the actual single-block phase, separate combat damage and end-combat observers, cleanup and snapshot replay.',
                            'Color predicates use independently pinned creature records. Threshold uses real owner graveyard size. No competing Lure or Banding claims.',
                        ]))

    def switch_turn(self):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.enemy)
        self.emit('turn', player=self.enemy, value=0)
        self.active, self.defender = self.enemy, self.owner



WITNESSES |= COMPOSITION_WITNESSES


def classify(record):
    try:
        return _pilot_parameters(record)
    except ValueError:
        return composition_parameters(record)
