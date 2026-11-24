"""Draw-step skips, replacement choices and production draw riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact

FAMILY = 'draw_step_skip_and_trigger'
PROFILE = Path(__file__).with_name('data') / 'draw_step_skip_and_trigger_wave8_profiles.json'
SKIP = 'Skip your draw step.'
RECYCLE = (SKIP, 'Whenever you play a card, draw a card.', 'Your maximum hand size is two.')
RECYCLE_MODES = ('own_spell', 'own_land', 'opposing_spell')
CASES = {
    11753: (('Deathtouch', SKIP, 'At the beginning of your upkeep, if you have fewer than seven cards in hand, '
             'draw cards equal to the difference.'), {'Deathtouch'}, ('refill', 'full', 'deathtouch')),
    8815: (RECYCLE, set(), RECYCLE_MODES),
    5443: ((SKIP, '{1}, Tap two untapped creatures you control: Draw a card.'),
           set(), ('activation', 'one_creature', 'fresh_payment')),
    6004: (('At the beginning of your upkeep, sacrifice this enchantment unless you discard a card.', SKIP,
            "You have shroud. (You can't be the target of spells or abilities.)",
            'Prevent all damage that would be dealt to you.'), set(), ('pay', 'empty')),
    3243: (RECYCLE, set(), RECYCLE_MODES),
    127: (('If you would draw a card during your draw step, instead you may skip that draw. If you do, until '
           "your next turn, you can't be attacked except by creatures with flying and/or islandwalk.",),
          set(), ('protect_expires', 'outside_draw')),
    317: (('At the beginning of your upkeep, put a hunger counter on this enchantment. Then destroy this '
           'enchantment if it has five or more hunger counters on it.',
           'If you would begin your draw step, you may skip that step instead. If you do, you gain 2 life.',
           'When you draw a card, destroy this enchantment.'), set(), ('skip_once', 'decline', 'hunger_five', 'outside_draw')),
}
ZONES = ('hand', 'battlefield', 'library', 'graveyard')
EMPTY = [0] * 6
LIBRARY = ('Island', 'Swamp', 'Forest', 'Plains', 'Mountain', 'Forest', 'Swamp', 'Island', 'Swamp')
BEAR = 'Grizzly Bears'
WALL = 'Wall of Stone'
MINE = 'Howling Mine'
SPELL = 'Divination'
RIFT = 'Flame Rift'
TARGETER = 'Duress'
AIR = 'Air Elemental'
BOA = 'River Boa'
DISENCHANT = 'Disenchant'
WITNESSES = {BEAR, WALL, MINE, SPELL, RIFT, TARGETER, AIR, BOA, DISENCHANT, *LIBRARY}
GAPS = {
    127: ['The player-one activated replacement is scoped to its actual deterministic seven-card condition. '
          'Explicit voluntary decline is scripted for player zero.'],
    317: ['There is no named hunger-counter observation. Actual own and opposing upkeep sequences establish '
          'survival through four and death on the fifth.'],
    3243: ['The headless turn and cleanup helpers omit the hand-limit discard loop.'],
    8815: ['The headless turn and cleanup helpers omit the hand-limit discard loop.'],
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in CASES or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face')):
        raise ValueError('whole normal skip-and-trigger source required')
    clauses, keywords, _ = CASES[csvid]
    if record.get('oracle_text', '').splitlines() != list(clauses) or set(record.get('keywords', ())) != keywords:
        raise ValueError('complete printed skip, replacement and rider text required')
    from recipes import cost_from_oracle

    return dict(csvid=csvid, casting_cost=cost_from_oracle(record), clauses=list(clauses))


def variants(shape):
    csvid = shape['csvid']
    all_seats = [f'{mode}_{seat}' for mode in CASES[csvid][2] for seat in (0, 1)]
    return all_seats + (['decline_0'] if csvid == 127 else [])


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported draw-step variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact full pinned draw-step primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('versioned whole draw-step declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES:
        raise ValueError('independent draw-step witnesses required')
    if {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(CASES):
        raise ValueError('complete draw-step source cohort required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or oracle[name].get('layout') != 'normal'):
                raise ValueError('draw-step source or witness changed: ' + name)
            if group == 'sources':
                if not str(row.get('handler', '')).startswith('manalinkeh_card_'):
                    raise ValueError('implemented draw-step primary required')
                classify(oracle[name], row['csvid'])
    if (oracle[AIR]['oracle_text'] != 'Flying' or 'Islandwalk' not in oracle[BOA]['keywords']
            or oracle[DISENCHANT]['oracle_text'] != 'Destroy target artifact or enchantment.'
            or oracle[SPELL]['oracle_text'] != 'Draw two cards.'):
        raise ValueError('actual draw, removal, flying and islandwalk witnesses required')


class Duel:
    def __init__(self, row, player, mode, rows, oracle):
        self.kind, self.name = row['csvid'], row['name']
        self.player, self.enemy, self.mode = player, 1 - player, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.life = [20, 20]
        self.draw_counts = [0, 0]
        self.hunger = 0
        self.active = player
        self.add('source', player, self.name, 'hand')
        for seat in (0, 1):
            for i, name in enumerate(LIBRARY):
                self.add(f'library_{seat}_{i}', seat, name, 'library')
        if self.kind == 11753:
            for i in range(7 if mode == 'full' else 3):
                self.add(f'keep{i}', player, 'Plains', 'hand')
            if mode == 'refill':
                self.add('mine', self.enemy, MINE, 'battlefield')
            if mode == 'deathtouch':
                self.add('wall', self.enemy, WALL, 'battlefield')
        if self.kind in (8815, 3243):
            if mode == 'own_land':
                self.add('land', player, 'Mountain', 'hand')
            else:
                self.add('spell', self.enemy if mode == 'opposing_spell' else player, SPELL, 'hand')
        if self.kind == 5443:
            self.add('bear_a', player, BEAR, 'battlefield')
            if mode != 'one_creature':
                self.add('bear_b', player, BEAR, 'hand' if mode == 'fresh_payment' else 'battlefield')
            self.add('foe_bear', self.enemy, BEAR, 'battlefield')
        if self.kind == 6004:
            if mode == 'pay':
                self.add('discard', player, 'Plains', 'hand')
            self.add('check_target', self.enemy, TARGETER, 'hand')
            self.add('rift', self.enemy, RIFT, 'hand')
            if mode == 'empty':
                self.add('second_rift', self.enemy, RIFT, 'hand')
        if self.kind in (127, 317) and mode == 'outside_draw':
            self.add('outside', player, SPELL, 'hand')
        if self.kind == 127 and mode != 'outside_draw':
            self.add('ground', self.enemy, BEAR, 'battlefield')
            self.add('air', self.enemy, AIR, 'battlefield')
            self.add('walk', self.enemy, BOA, 'battlefield')
            self.add('island', self.player, 'Island', 'battlefield')
            if mode == 'protect_expires':
                self.add('removal', player, DISENCHANT, 'hand')
                if player == 1:
                    # Seven actual cards make the AI's rand(8 - hand) zero.
                    for i in range(6):
                        self.add(f'keep{i}', player, 'Plains', 'hand')

    def add(self, ref, player, name, zone):
        from recipes import card

        self.cards[ref] = [player, name, zone]
        self.setup.append(card(name, ref, player, zone))

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        from recipes import step

        if op in ('untap_step', 'upkeep_step', 'draw_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, ref=ref, player=player))
            return
        fields = dict(player=player, value=value, index=index, color=color)
        if ref:
            fields['ref'] = ref
        if other:
            fields['other'] = other
        if prop:
            fields['property'] = prop
        if op.startswith('assert_zone'):
            fields['zone'] = ZONES[zone]
        self.steps.append(step(op, **fields))

    def card(self, ref, prop, expected):
        self.emit('assert_card', ref, prop=prop, value=expected)

    def pool(self, op, player, pool):
        for color, value in enumerate(pool):
            self.emit(op, player=player, color=color, value=value)

    def empty(self):
        for player in (0, 1):
            self.pool('assert_mana', player, EMPTY)

    def zone(self, ref, destination):
        self.cards[ref][2] = destination

    def draw(self, player, count):
        for _ in range(count):
            i = self.draw_counts[player]
            self.zone(f'library_{player}_{i}', 'hand')
            self.draw_counts[player] += 1

    def receipts(self):
        self.emit('assert_stack', value=0)
        for seat in (0, 1):
            self.emit('assert_life', player=seat, value=self.life[seat])
            names = sorted({name for owner, name, _ in self.cards.values() if owner == seat})
            for number, zone in enumerate(ZONES):
                contents = [name for owner, name, where in self.cards.values() if (owner, where) == (seat, zone)]
                self.emit('assert_zone', player=seat, zone=number, value=len(contents))
                totals = Counter(contents)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=seat, zone=number,
                              index=self.rows[name]['csvid'], value=totals[name])
                if zone == 'library':
                    for index, name in enumerate(contents):
                        self.emit('assert_zone_card', player=seat, zone=number, index=index,
                                  value=self.rows[name]['csvid'])

    def cast(self, ref, destination, *, announce=False, target=None):
        from recipes import cost_from_oracle

        seat, name, where = self.cards[ref]
        assert where == 'hand' and seat == self.active
        self.pool('set_mana', seat, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target is not None:
            self.emit('choose_target', target, player=seat)
        self.emit('cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        if announce:
            self.emit('announce_spell', ref)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.zone(ref, destination)
        self.empty()

    def turn(self):
        self.emit('turn', player=self.active, value=0)

    def visit(self, *, replace_draw=False, fasting_skip=False):
        self.empty()
        self.emit('advance_turn')
        self.active = 1 - self.active
        self.emit('untap_step', player=self.active)
        owner_turn = self.active == self.player
        source_live = self.cards['source'][2] == 'battlefield'
        if self.kind == 6004 and owner_turn and source_live and self.mode == 'pay':
            if self.active == 0:
                self.emit('discard_policy', value=1)
            else:
                self.emit('choose_zone_card', player=1, value=self.rows['Plains']['csvid'])
            self.zone('discard', 'graveyard')
        self.emit('upkeep_step', player=self.active)
        self.emit('assert_choices_consumed')
        if self.kind == 11753 and owner_turn and source_live:
            hand = sum(seat == self.player and zone == 'hand' for seat, _, zone in self.cards.values())
            self.draw(self.player, max(0, 7 - hand))
        if self.kind == 6004 and owner_turn and source_live:
            if self.mode == 'empty':
                self.zone('source', 'graveyard')
                source_live = False
            elif self.active == 0:
                self.emit('assert_discard_choices', value=1)
        if self.kind == 317 and owner_turn and source_live:
            self.hunger += 1
            if self.hunger >= 5:
                self.zone('source', 'graveyard')
                source_live = False
        self.empty()
        self.receipts()
        if owner_turn and source_live and self.kind == 127 and replace_draw and self.player == 0:
            self.emit('choose_trigger_source', 'source', player=self.player)
        if owner_turn and source_live and self.kind == 317:
            self.emit('choose_dialog', player=self.player, value=int(not fasting_skip))
        self.emit('draw_step', player=self.active)
        self.emit('assert_choices_consumed')
        if owner_turn and source_live and self.kind == 317 and fasting_skip:
            self.life[self.player] += 2
        elif (not owner_turn or not source_live or self.kind == 317
              or (self.kind == 127 and not replace_draw)):
            self.draw(self.active, 2 if 'mine' in self.cards else 1)
            if self.kind == 317 and owner_turn and source_live:
                self.zone('source', 'graveyard')
        self.receipts()

    def damia_deathtouch(self):
        self.turn()
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', 'wall', 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        self.zone('wall', 'graveyard')
        self.receipts()

    def profusion(self):
        if self.mode != 'opposing_spell':
            self.visit()
        self.turn()
        if self.mode == 'own_land':
            self.emit('play_land', 'land', value=1)
            self.emit('resolve')
            self.emit('refresh')
            self.emit('assert_choices_consumed')
            self.zone('land', 'battlefield')
            self.draw(self.player, 1)
        else:
            self.cast('spell', 'graveyard', announce=True)
            self.draw(self.active, 2 + int(self.active == self.player))
        self.receipts()
        if self.mode == 'opposing_spell':
            self.visit()

    def deployment(self):
        self.visit()
        self.turn()
        if self.mode == 'fresh_payment':
            self.cast('bear_b', 'battlefield')
            self.card('bear_b', 'sick', 1)
        self.emit('can_activate', 'source', value=0)
        self.pool('set_mana', self.player, [1, 0, 0, 0, 0, 0])
        self.emit('refresh')
        payable = self.mode != 'one_creature'
        self.emit('can_activate', 'source', value=int(payable))
        if payable:
            self.emit('choose_target', 'bear_a', player=self.player)
            self.emit('choose_target', 'bear_b', player=self.player)
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.emit('assert_choices_consumed')
            self.card('bear_a', 'tapped', 1)
            self.card('bear_b', 'tapped', 1)
            self.card('foe_bear', 'tapped', 0)
            self.receipts_pending_activation()
            self.emit('resolve')
            self.emit('refresh')
            self.draw(self.player, 1)
            self.empty()
        else:
            self.card('bear_a', 'tapped', 0)
            self.card('foe_bear', 'tapped', 0)
            self.emit('assert_mana', player=self.player, color=0, value=1)
        self.receipts()

    def receipts_pending_activation(self):
        current = sum(seat == self.player and zone == 'hand' for seat, _, zone in self.cards.values())
        self.emit('assert_zone', player=self.player, zone=ZONES.index('hand'), value=current)
        self.emit('assert_zone', player=self.player, zone=ZONES.index('library'),
                  value=len(LIBRARY) - self.draw_counts[self.player])

    def confinement(self, ref, protected):
        self.turn()
        self.emit('can_cast', 'check_target', value=int(not protected))
        self.cast(ref, 'graveyard')
        self.life[self.enemy] -= 4
        if not protected:
            self.life[self.player] -= 4
        self.receipts()

    def sanctuary_combat(self, protected):
        assert self.active == self.enemy
        self.turn()
        self.emit('begin_attackers')
        self.emit('attack', 'ground', value=int(not protected))
        self.emit('attack', 'air', value=1)
        self.emit('attack', 'walk', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('end_combat')
        damage = sum(int(self.oracle[name]['power']) for name in (AIR, BOA))
        if not protected:
            damage += int(self.oracle[BEAR]['power'])
        self.life[self.player] -= damage
        self.receipts()

    def sanctuary(self):
        protected = self.mode == 'protect_expires'
        self.visit(replace_draw=protected)
        if protected:
            # Printed attack protection continues after the enchantment dies.
            self.turn()
            self.cast('removal', 'graveyard', target='source')
            self.zone('source', 'graveyard')
            self.receipts()
        self.visit()
        if self.mode != 'outside_draw':
            self.sanctuary_combat(protected=protected)
        if protected:
            self.visit()
            self.visit()
            self.sanctuary_combat(protected=False)

    def fasting(self):
        accepting = self.mode in ('skip_once', 'hunger_five')
        self.visit(fasting_skip=accepting)
        if self.mode == 'hunger_five':
            for i in range(4):
                self.visit()
                self.visit(fasting_skip=i < 3)

    def outside_draw(self):
        self.cast('outside', 'graveyard')
        self.draw(self.player, 2)
        if self.kind == 317:
            self.zone('source', 'graveyard')
        self.receipts()

    def build(self):
        self.turn()
        self.pool('set_mana', self.enemy, EMPTY)
        self.cast('source', 'battlefield')
        self.card('source', 'in_play', 1)
        self.receipts()
        if self.mode == 'outside_draw':
            self.outside_draw()
        if self.kind == 317:
            self.emit('present_ai_dialogs', value=1)
        self.visit()  # The first real opponent draw is still available.
        if self.kind == 11753:
            self.visit()
            if self.mode == 'deathtouch':
                self.damia_deathtouch()
        elif self.kind in (8815, 3243):
            self.profusion()
        elif self.kind == 5443:
            self.deployment()
        elif self.kind == 6004:
            self.confinement('rift', protected=True)
            self.visit()
            if self.mode == 'empty':
                self.visit()
                self.confinement('second_rift', protected=False)
        elif self.kind == 127:
            self.sanctuary()
        elif self.kind == 317:
            self.fasting()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact registered draw-step primary CSVID required')
    output = []
    for variant in selected:
        mode, actor = variant.rsplit('_', 1)
        game = Duel(rows[name], int(actor), mode, rows, oracle)
        game.build()
        output.append(dict(
            name=f'DrawStepSkipAndTriggerRecipe_{csvid}_{variant}', card=name, fixture='empty_main', seed=2528,
            setup=game.setup, steps=game.steps,
            _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                         oracle_text=declaration['oracle_text'], untested_components=GAPS.get(csvid, [])),
        ))
    return output



