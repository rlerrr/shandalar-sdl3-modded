"""Whole draw-step effects with real returns, controls, and loyalty effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'draw_step_return_control_and_board'
PROFILE = Path(__file__).with_name('data') / 'draw_step_return_control_and_board_wave8_profiles.json'
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = [0] * 6

GARGOYLE = 2300
FIREBIRD = 8810
AUTHORITY = 12802
SUN = 18314
RETURN = "When this creature dies, return it to the battlefield under its owner's control at the beginning of the next end step and you skip your next draw step."
PRINTED = {
    GARGOYLE: ('{4}{W}', 'Creature — Gargoyle', ('Flying',), ('Flying', RETURN, '{4}{W}: Exile this creature.')),
    FIREBIRD: ('{4}{R}', 'Creature — Phoenix', ('Flying',), ('Flying', RETURN, '{4}{R}: Exile this creature.')),
    AUTHORITY: ('{3}{W}{U}', 'Enchantment — Aura', ('Enchant',), (
        'Enchant creature',
        "Enchanted creature gets +1/+1 for each card in its controller's hand.",
        "At the beginning of the draw step of enchanted creature's controller, that player draws an additional card.",
    )),
    SUN: ('{6}', 'Legendary Artifact', (), (
        "Players can't activate planeswalkers' loyalty abilities.",
        'At the beginning of your draw step, draw an additional card.',
        'Spells you cast cost {1} less to cast.',
        'Creatures you control get +1/+1.',
    )),
}
MODES = {
    GARGOYLE: ('death_return', 'stolen_death', 'self_exile'),
    FIREBIRD: ('death_return', 'stolen_death', 'self_exile'),
    AUTHORITY: ('own_host', 'foe_host', 'disenchanted'),
    SUN: ('draw_and_anthem', 'loyalty_lock', 'cost_scope'),
}

SARKHAN = 14364
PRINTED[SARKHAN] = ('{3}{R}{R}', 'Legendary Planeswalker — Sarkhan', (), (
    "+1: Until end of turn, Sarkhan becomes a legendary 4/4 red Dragon creature with flying, indestructible, and haste. (He doesn't lose loyalty while he's not a planeswalker.)",
    '−3: Sarkhan deals 4 damage to target creature.',
    '−6: You get an emblem with "At the beginning of your draw step, draw two additional cards" and "At the beginning of your end step, discard your hand."',
))
MODES[SARKHAN] = ('animation', 'damage_lethal', 'damage_marked', 'emblem')
NEW_WITNESSES = {'Lightning Bolt', 'Day of Judgment', 'Crucible of Fire'}
WITNESSES = {
    'Forest', 'Island', 'Mountain', 'Plains', 'Swamp', 'Doom Blade', 'Control Magic',
    'Dazzling Ramparts', 'Prodigal Sorcerer', 'Disenchant', 'Jace Beleren', 'Naturalize', 'Divination',
} | NEW_WITNESSES
GAPS = {
    SARKHAN: ['Emblem exists through its real repeated effects; the visible permanent observer hides nonpermanent effects.',
              'Legendary status is pinned but no same-name legend rule collision is scheduled.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in PRINTED or not record or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal draw-step primary required')
    fee, type_line, keywords, clauses = PRINTED[csvid]
    if (record.get('layout') != 'normal' or record.get('type_line') != type_line
            or record.get('mana_cost') != fee or tuple(record.get('keywords', ())) != keywords
            or record.get('oracle_text') != '\n'.join(clauses)):
        raise ValueError('complete pinned printed costs and draw-step clauses required')
    if csvid in (GARGOYLE, FIREBIRD) and (record.get('power'), record.get('toughness')) != ('2', '2'):
        raise ValueError('reviewed printed nontoken creature stats required')
    if csvid == SARKHAN and record.get('loyalty') != '4':
        raise ValueError('reviewed printed starting loyalty required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(m) is not str for m in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('explicit distinct draw-step variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name', ''))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('complete pinned draw-step source required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('version 2 whole-card draw-step declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {p['registry']['csvid'] for p in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete draw-step source and witness pins required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows.get(name, {})
        if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or oracle[name].get('layout') != 'normal'):
            raise ValueError('pinned draw-step source or witness changed: ' + name)
        if name in pins['sources']:
            classify(oracle[name], row['csvid'])


class Duel:
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.active = actor
        self.animated = False
        self.loyalty = int(record.get('loyalty', 0))
        self.drawn = [0, 0]
        self.library = {
            actor: 'Forest Island Mountain Plains Swamp Forest Mountain Swamp Island'.split(),
            self.foe: 'Plains Swamp Island Forest Mountain Plains Forest Mountain Swamp'.split(),
        }
        self.add('source', actor, record['name'], 'hand')
        for seat, names in self.library.items():
            for i, name in enumerate(names):
                self.add(f'd_{seat}_{i}', seat, name, 'library')
        if mode in ('death_return', 'stolen_death'):
            self.add('kill', actor if mode == 'stolen_death' else self.foe, 'Doom Blade', 'hand')
            if mode == 'stolen_death':
                self.add('control', self.foe, 'Control Magic', 'hand')
        if mode == 'self_exile':
            self.add('ground_blocker', self.foe, 'Dazzling Ramparts', 'battlefield')
        if csvid == AUTHORITY:
            self.host = self.foe if mode == 'foe_host' else actor
            self.add('host', self.host, 'Dazzling Ramparts', 'battlefield')
            self.add('bystander', 1 - self.host, 'Prodigal Sorcerer', 'battlefield')
            self.add('own_seed_0', actor, 'Forest', 'hand')
            self.add('own_seed_1', actor, 'Plains', 'hand')
            self.add('foe_seed_0', self.foe, 'Mountain', 'hand')
            if mode == 'disenchanted':
                self.add('remove', self.foe, 'Disenchant', 'hand')
        if csvid == SUN:
            if mode == 'draw_and_anthem':
                self.add('ally', actor, 'Prodigal Sorcerer', 'battlefield')
                self.add('enemy', self.foe, 'Dazzling Ramparts', 'battlefield')
            if mode == 'loyalty_lock':
                for seat in (actor, self.foe):
                    self.add(f'walker_{seat}', seat, 'Jace Beleren', 'battlefield')
                self.add('remove', self.foe, 'Naturalize', 'hand')
            if mode == 'cost_scope':
                self.add('my_divination', actor, 'Divination', 'hand')
                self.add('foe_divination', self.foe, 'Divination', 'hand')
                self.add('post_divination', actor, 'Divination', 'hand')
                self.add('remove', actor, 'Naturalize', 'hand')

        if csvid == SARKHAN:
            if mode == 'animation':
                self.add('ground_blocker', self.foe, 'Dazzling Ramparts', 'battlefield')
                self.add('strike', self.foe, 'Lightning Bolt', 'hand')
                self.add('judgment', actor, 'Day of Judgment', 'hand')
                self.add('dragon_lord', actor, 'Crucible of Fire', 'hand')
            elif mode.startswith('damage_'):
                target = 'Prodigal Sorcerer' if mode == 'damage_lethal' else 'Dazzling Ramparts'
                self.add('loyalty_target', self.foe, target, 'battlefield')
            else:
                self.add('loyalty_target', self.foe, 'Dazzling Ramparts', 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'draw_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op in ('assert_zone', 'assert_zone_card', 'assert_zone_multiplicity'):
            fields['zone'] = next(name for name, number in ZONE.items() if number == zone)
        self.steps.append(step(op, **fields))

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def mana(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def receipts(self, stack=0):
        self.emit('assert_stack', value=stack)
        names = sorted({c['name'] for c in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone, index in ZONE.items():
                contents = self.library[player] if zone == 'library' else [
                    c['name'] for c in self.cards.values() if c['player'] == player and c['zone'] == zone
                ]
                self.emit('assert_zone', player=player, zone=index, value=len(contents))
                occurrences = Counter(contents)
                for name in names:
                    if self.animated and zone == 'battlefield' and name == self.record['name']:
                        continue
                    self.emit('assert_zone_multiplicity', player=player, zone=index,
                              index=self.rows[name]['csvid'], value=occurrences[name])
                if zone == 'library':
                    for i, name in enumerate(contents):
                        self.emit('assert_zone_card', player=player, zone=index, index=i,
                                  value=self.rows[name]['csvid'])

    def checks(self, *, stack=0):
        self.receipts(stack)
        for seat in (0, 1):
            self.mana('assert_mana', seat, ZERO)

    def cast_card(self, ref, *, target=None, battlefield=False, discount=0):
        row = self.cards[ref]
        seat = row['player']
        fee = cost_from_oracle(self.oracle[row['name']])
        fee[0] -= discount
        assert fee[0] >= 0
        self.mana('set_mana', seat, fee)
        self.finish_cast(ref, target=target, battlefield=battlefield)

    def finish_cast(self, ref, *, target=None, battlefield=False):
        row = self.cards[ref]
        seat = row['player']
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', seat, ZERO)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        row['zone'] = 'battlefield' if battlefield else 'graveyard'

    def mark_draws(self, seat, count):
        assert len(self.library[seat]) >= count
        for _ in range(count):
            self.cards[f'd_{seat}_{self.drawn[seat]}']['zone'] = 'hand'
            self.drawn[seat] += 1
            del self.library[seat][0]

    def next_draw(self, count, *, return_at_end=False, emblem_discards=False):
        discarded = []
        if emblem_discards:
            discarded = [entry for entry in self.cards.values()
                         if entry['player'] == self.actor and entry['zone'] == 'hand']
            assert self.active == self.actor and 0 < len(discarded) <= 7
            if self.actor == 0:
                # The human discard-all callback presents each remaining hand card.
                for entry in discarded:
                    self.emit('choose_zone_card', player=self.actor, value=self.rows[entry['name']]['csvid'])
        self.active = 1 - self.active
        self.emit('advance_turn')
        self.animated = False
        if emblem_discards:
            for entry in discarded:
                entry['zone'] = 'graveyard'
            self.checks()
        if return_at_end:
            self.cards['source'].update(player=self.actor, zone='battlefield')
            self.checks()
        self.emit('untap_step', player=self.active)
        self.emit('upkeep_step', player=self.active)
        self.checks()
        self.emit('draw_step', player=self.active)
        self.emit('assert_choices_consumed')
        self.mark_draws(self.active, count)
        self.checks()
        self.emit('turn', player=self.active)

    def death_return(self):
        if self.mode == 'stolen_death':
            self.next_draw(1)
            self.cast_card('control', target='source', battlefield=True)
            self.cards['source']['player'] = self.foe
            self.checks()
            self.emit('bind_permanent_copy', 'stolen', player=self.foe, value=self.csvid)
            self.observe('stolen', 'in_play', 1)
            victim = 'stolen'
        else:
            victim = 'source'
        self.cast_card('kill', target=victim)
        self.cards['source'].update(player=self.actor, zone='graveyard')
        if self.mode == 'stolen_death':
            self.cards['control']['zone'] = 'graveyard'
        self.checks()
        self.next_draw(1, return_at_end=True)
        self.next_draw(0)
        self.next_draw(1)
        self.next_draw(1)

    def flying_then_exile(self):
        self.next_draw(1)
        self.next_draw(1)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground_blocker', 'source', value=0)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= int(self.record['power'])
        self.checks()
        self.emit('can_activate', 'source', value=0)
        exile_cost = self.record['oracle_text'].splitlines()[-1].partition(':')[0]
        self.mana('set_mana', self.actor, cost_from_oracle({'mana_cost': exile_cost}))
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.observe('source', 'in_play', 1)
        self.checks(stack=1)
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.cards['source']['zone'] = 'exile'
        self.checks()
        self.next_draw(1)
        self.next_draw(1)

    def pts(self, ref, bonus):
        oracle = self.oracle[self.cards[ref]['name']]
        self.observe(ref, 'power', int(oracle['power']) + bonus)
        self.observe(ref, 'toughness', int(oracle['toughness']) + bonus)

    def hand_size(self, seat):
        return sum(item['player'] == seat and item['zone'] == 'hand' for item in self.cards.values())

    def authority_stats(self):
        self.emit('refresh')
        attached = self.cards['source']['zone'] == 'battlefield'
        self.pts('host', self.hand_size(self.host) if attached else 0)
        self.pts('bystander', 0)

    def authority(self):
        self.cast_card('source', target='host', battlefield=True)
        self.checks()
        self.authority_stats()
        self.next_draw(2 if self.host == self.foe else 1)
        self.authority_stats()
        if self.mode == 'disenchanted':
            self.cast_card('remove', target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.checks()
            self.authority_stats()
        self.next_draw(2 if self.host == self.actor and self.mode != 'disenchanted' else 1)
        self.authority_stats()

    def walker(self, seat, eligible):
        ref = f'walker_{seat}'
        self.emit('refresh')
        self.emit('can_activate', ref, value=int(eligible))
        self.emit('assert_counter', ref, prop='loyalty', value=int(self.oracle['Jace Beleren']['loyalty']))

    def prove_full_fee(self, ref):
        seat = self.cards[ref]['player']
        printed = cost_from_oracle(self.oracle['Divination'])
        short = list(printed)
        short[0] -= 1
        self.mana('set_mana', seat, short)
        self.emit('refresh')
        self.emit('cast', ref, value=0)
        self.emit('assert_stack', value=0)
        self.observe(ref, 'in_hand', 1)
        self.mana('assert_mana', seat, short)
        self.emit('assert_choices_consumed')
        self.emit('set_mana', player=seat, color=0, value=printed[0])
        self.finish_cast(ref)
        self.mark_draws(seat, 2)
        self.checks()

    def sun(self):
        if self.mode == 'loyalty_lock':
            self.walker(self.actor, True)
        self.cast_card('source', battlefield=True)
        self.checks()
        if self.mode == 'draw_and_anthem':
            self.pts('ally', 1)
            self.pts('enemy', 0)
            self.next_draw(1)
            self.next_draw(2)
            self.pts('ally', 1)
            self.pts('enemy', 0)
        elif self.mode == 'loyalty_lock':
            self.walker(self.actor, False)
            self.next_draw(1)
            self.walker(self.foe, False)
            self.cast_card('remove', target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.checks()
            self.walker(self.foe, True)
            self.next_draw(1)
            self.walker(self.actor, True)
        else:
            self.cast_card('my_divination', discount=1)
            self.mark_draws(self.actor, 2)
            self.checks()
            self.next_draw(1)
            self.prove_full_fee('foe_divination')
            self.next_draw(2)
            self.cast_card('remove', target='source', discount=1)
            self.cards['source']['zone'] = 'graveyard'
            self.checks()
            self.prove_full_fee('post_divination')

    def observe_loyalty(self):
        self.emit('assert_counter', 'source', prop='loyalty', value=self.loyalty)

    def select_loyalty(self, mode, fee, *, target=None):
        self.emit('can_activate', 'source', value=1)
        self.emit('present_ai_dialogs', value=1)
        self.emit('choose_dialog', player=self.actor, value=mode)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.loyalty += fee
        assert self.loyalty >= 0
        if self.loyalty:
            self.observe_loyalty()
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('present_ai_dialogs', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')
        self.emit('refresh')

    def animate(self):
        self.select_loyalty(0, 1)
        self.animated = True
        self.emit('can_activate', 'source', value=0)
        self.observe('source', 'in_play', 1)
        self.observe('source', 'power', 4)
        self.observe('source', 'toughness', 4)
        self.observe('source', 'color', 1 << 4)
        self.observe_loyalty()
        self.checks()

    def animation_combat(self):
        self.emit('can_target_creature', 'strike', 'source', value=0)
        self.animate()
        self.emit('can_target_creature', 'strike', 'source', value=1)
        self.cast_card('strike', target='source')
        self.observe('source', 'damage', 3)
        self.observe_loyalty()
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'ground_blocker', 'source', value=0)
        self.emit('advance_combat')
        self.observe('source', 'blocked', 0)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.foe] -= 4
        self.checks()
        self.cast_card('dragon_lord', battlefield=True)
        self.observe('source', 'power', 7)
        self.observe('source', 'toughness', 7)
        self.cast_card('judgment')
        self.cards['ground_blocker']['zone'] = 'graveyard'
        self.observe('source', 'in_play', 1)
        self.observe_loyalty()
        self.checks()
        self.next_draw(1)
        self.emit('can_target_creature', 'source', 'source', value=0)
        self.observe_loyalty()

    def sarkhan(self):
        self.cast_card('source', battlefield=True)
        self.observe_loyalty()
        self.checks()
        if self.mode == 'animation':
            self.animation_combat()
        elif self.mode.startswith('damage_'):
            self.select_loyalty(1, -3, target='loyalty_target')
            if self.mode == 'damage_lethal':
                self.cards['loyalty_target']['zone'] = 'graveyard'
            else:
                self.observe('loyalty_target', 'in_play', 1)
                self.observe('loyalty_target', 'damage', 4)
            self.observe_loyalty()
            self.emit('can_activate', 'source', value=0)
            self.checks()
        else:
            while self.loyalty < 6:
                self.animate()
                self.next_draw(1)
                self.observe_loyalty()
                self.next_draw(1)
            self.select_loyalty(2, -6)
            self.cards['source']['zone'] = 'graveyard'
            self.checks()
            for _ in range(2):
                self.next_draw(1, emblem_discards=True)
                self.next_draw(3)
            self.next_draw(1, emblem_discards=True)

    def build(self):
        self.emit('turn', player=self.actor)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.checks()
        if self.csvid == AUTHORITY:
            self.authority()
        elif self.csvid == SUN:
            self.sun()
        elif self.csvid == SARKHAN:
            self.sarkhan()
        else:
            self.cast_card('source', battlefield=True)
            self.checks()
            self.pts('source', 0)
            if self.mode == 'self_exile':
                self.flying_then_exile()
            else:
                self.death_return()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered draw-step primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        duel = Duel(csvid, oracle[name], int(seat), mode, rows, oracle)
        duel.build()
        cases.append(dict(
            name=f'DrawReturnControlBoardRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2528,
            setup=duel.setup, steps=duel.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return cases
