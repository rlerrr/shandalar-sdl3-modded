"""Paid draw-step damage and independent upkeep, payment, and combat riders."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step


FAMILY = 'draw_step_damage_and_upkeep'
PROFILE = Path(__file__).with_name('data') / 'draw_step_damage_and_upkeep_wave8_profiles.json'
CLOCK, VAULT, NEKUSAR, VISIONS, ASP, MASTICORE = 471, 157, 13627, 9684, 444, 7106
MODES = {
    CLOCK: ('scales', 'removed', 'replay', 'prevention'),
    VAULT: ('tapped', 'untapped', 'short', 'pay', 'prevention'),
    NEKUSAR: ('both_steps', 'spell_draw', 'removed', 'replay', 'prevention'),
    VISIONS: ('both_steps', 'spell_draw', 'removed', 'replay', 'prevention'),
    ASP: ('short', 'paid', 'fog', 'blocked', 'removed_after_hit', 'life_loss'),
    MASTICORE: ('foe_target', 'own_target', 'prevented_target', 'first_strike', 'empty', 'upkeep_decline'),
}
HUMAN_MODES = {CLOCK: ('upkeep_payment',), VAULT: ('decline',), MASTICORE: ('decline_shot',)}
ZONE = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZONES = {index: name for name, index in ZONE.items()}
DECKS = (('Island', 'Mountain', 'Swamp', 'Plains', 'Forest', 'Mountain'),
         ('Swamp', 'Forest', 'Island', 'Mountain', 'Plains', 'Forest'))

WITNESSES = ('Cho-Manno, Revolutionary', 'Disenchant', 'Divination', 'Enormous Baloth', 'Fog', 'Forest', 'Grizzly Bears', 'Island', 'Mountain', 'Murder', 'Patchwork Gnomes', 'Personal Sanctuary', 'Plains', 'Shock', 'Swamp')
PRINTED = {471: ('{6}', 'Artifact'), 157: ('{1}', 'Artifact'), 13627: ('{2}{U}{B}{R}', 'Legendary Creature'), 9684: ('{2}{B/R}{B/R}', 'Enchantment'), 444: ('{G}', 'Creature'), 7106: ('{5}', 'Artifact Creature')}

GAPS = {
    CLOCK: ('Doom progression is measured by exact later damage. The actor-aware removal probe awaits '
            'a verified response-window path, and an opposing player activating the Clock is outside current coverage.',),
    VAULT: ('The human paid and decline cases are explicit; the AI paid fixture offers enough resources for its policy.',),
    ASP: ('A real one-mana source is offered as a scripted payment intention before entry. The implementation '
          'has no independent prevention may-pay window before the draw; its phase handler is diagnosed separately.',),
    MASTICORE: ('A human decline is explicit. AI trigger policy is not treated as a choice guarantee.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('normal whole draw-step primary required')
    mana, card_type = PRINTED[csvid]
    if record.get('mana_cost') != mana or record.get('type_line', '').split(' — ')[0] != card_type:
        raise ValueError('unsupported draw-step primary cost or type')
    return dict(csvid=csvid, printed_cost=mana)


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{actor}' for actor in (0, 1)
            for mode in (*MODES[csvid], *(HUMAN_MODES.get(csvid, ()) if actor == 0 else ()))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct supported draw-step variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned whole draw-step primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed draw-step declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != set(WITNESSES)
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != PRINTED.keys()):
        raise ValueError('exact draw-step source and witness scope required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('draw-step Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented draw-step primary required')
                classify(oracle[name], row['csvid'])


class Board:
    def __init__(self, csvid, actor, mode, rows, oracle):
        self.csvid, self.actor, self.foe, self.mode = csvid, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.name = next(row['name'] for row in rows.values() if row['csvid'] == csvid)
        self.setup, self.steps, self.cards = [], [], {}
        self.turn = actor
        self.library = [list(cards) for cards in DECKS]
        self.hand = [[], []]
        self.life = [20, 20]
        self.doom = 0
        self.vault_tapped = 0
        self.funding = []
        self.add('source', actor, self.name, 'hand')
        for player, deck in enumerate(self.library):
            for index, name in enumerate(deck):
                self.add(f'library_{player}_{index}', player, name, 'library')
        if mode == 'removed':
            self.add('removal', self.foe, 'Murder' if csvid == NEKUSAR else 'Disenchant', 'hand')
        if mode == 'spell_draw':
            self.add('own_draw', self.actor, 'Divination', 'hand')
            self.add('foe_draw', self.foe, 'Divination', 'hand')
        if csvid == VAULT and mode != 'untapped':
            self.add('mana_sink', actor, 'Patchwork Gnomes', 'hand')
            counts = {'short': 3, 'decline': 4, 'pay': 6 if actor == 1 else 4}
            for index in range(counts.get(mode, 0)):
                slot = f'payment_land_{index}'
                self.add(slot, actor, 'Mountain', 'battlefield')
                self.funding.append(slot)

        if csvid == CLOCK and mode == 'upkeep_payment':
            self.add('clock_damage', actor, 'Shock', 'hand')
        if mode == 'prevention' or (csvid == ASP and mode == 'life_loss'):
            protected = self.foe if csvid in (ASP, NEKUSAR) else self.actor
            self.add('sanctuary', protected, 'Personal Sanctuary', 'battlefield')
        if csvid == ASP:
            if mode == 'paid':
                self.add('asp_payment_land', self.foe, 'Mountain', 'battlefield')
            elif mode == 'fog':
                self.add('fog', self.foe, 'Fog', 'hand')
            elif mode == 'blocked':
                self.add('blocker', self.foe, 'Grizzly Bears', 'battlefield')
            elif mode == 'removed_after_hit':
                self.add('asp_removal', self.foe, 'Murder', 'hand')
        if csvid == MASTICORE:
            if mode != 'empty':
                self.add('discard_card', self.actor, 'Swamp', 'hand')
            if mode in ('foe_target', 'decline_shot'):
                self.add('shot_target', self.foe, 'Grizzly Bears', 'battlefield')
            elif mode == 'own_target':
                self.add('shot_target', self.actor, 'Enormous Baloth', 'battlefield')
            elif mode == 'prevented_target':
                self.add('shot_target', self.foe, 'Cho-Manno, Revolutionary', 'battlefield')
            elif mode == 'first_strike':
                self.add('shot_target', self.foe, 'Enormous Baloth', 'battlefield')
                self.add('blocker', self.foe, 'Grizzly Bears', 'battlefield')

    def add(self, ref, player, name, zone):
        self.setup.append(card(name, ref, player, zone))
        if zone != 'library':
            self.cards[ref] = dict(name=name, player=player, zone=zone)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step', 'draw_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
            return
        if op == 'choose_trigger_source':
            self.steps.append(step(op, player=player, ref=ref))
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

    def mana(self, op, player, amounts=None):
        for color, amount in enumerate(amounts if amounts is not None else (0,) * 6):
            self.emit(op, player=player, color=color, value=amount)

    def observe(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def choices(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def receipts(self):
        self.emit('assert_stack')
        self.choices()
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            self.mana('assert_mana', player)
            for zone, zone_id in ZONE.items():
                if zone == 'library':
                    names = self.library[player]
                else:
                    names = [obj['name'] for obj in self.cards.values()
                             if obj['player'] == player and obj['zone'] == zone]
                    if zone == 'hand':
                        names += self.hand[player]
                self.emit('assert_zone', player=player, zone=zone_id, value=len(names))
                for name, count in Counter(names).items():
                    self.emit('assert_zone_multiplicity', player=player, zone=zone_id,
                              index=self.rows[name]['csvid'], value=count)
                if zone == 'library':
                    for index, name in enumerate(names):
                        self.emit('assert_zone_card', player=player, zone=zone_id, index=index,
                                  value=self.rows[name]['csvid'])
        if self.cards['source']['zone'] == 'battlefield':
            self.observe('source', 'in_play', 1)
            if self.csvid == VAULT:
                self.observe('source', 'tapped', self.vault_tapped)
            elif self.csvid == NEKUSAR:
                self.observe('source', 'power', 2)
                self.observe('source', 'toughness', 4)
            elif self.csvid in (ASP, MASTICORE):
                stats = 1 if self.csvid == ASP else 5
                self.observe('source', 'power', stats)
                self.observe('source', 'toughness', stats)

    def present(self):
        return self.cards['source']['zone'] == 'battlefield'

    def cast(self, ref, destination, *, target=None, picked=None, fund=True):
        obj = self.cards[ref]
        name, player = obj['name'], obj['player']
        if fund:
            record = self.oracle[name]
            if name == 'Spiteful Visions':
                printed = record['mana_cost']
                assert printed == '{2}{B/R}{B/R}'
                record = dict(record, mana_cost=printed.replace('{B/R}', '{B}' if player == 0 else '{R}'))
            self.mana('set_mana', player, cost_from_oracle(record))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        if picked is not None:
            self.emit('choose_player', player=player, value=picked)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', player)
        self.emit('resolve')
        self.choices()
        obj['zone'] = destination

    def untap(self, player):
        assert self.turn == 1 - player
        self.turn = player
        self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.receipts()

    def upkeep(self, player, *, vault_pay=False):
        assert self.turn == player
        if vault_pay and player == 0:
            self.emit('choose_trigger_source', 'source', player=player)
            for ref in self.funding[:4]:
                self.emit('choose_mana_source', ref)
        self.emit('upkeep_step', player=player)
        if self.csvid == CLOCK and self.present() and player == self.actor:
            self.doom += 1
        if vault_pay:
            self.vault_tapped = 0
            if player == 0:
                for ref in self.funding:
                    self.observe(ref, 'tapped', 1)
        elif self.csvid == VAULT and player == self.actor:
            for ref in self.funding:
                self.observe(ref, 'tapped', 0)
        self.receipts()

    def prevented(self, player):
        ward = self.cards.get('sanctuary')
        return bool(ward and ward['player'] == player and self.turn == player)

    def draw_cards(self, player, number):
        for _ in range(number):
            self.hand[player].append(self.library[player].pop(0))
        if self.present():
            if (self.csvid == VISIONS or (self.csvid == NEKUSAR and player == self.foe)) and not self.prevented(player):
                self.life[player] -= number

    def draw(self, player, *, replay=False, before_receipts=None):
        assert self.turn == player
        if replay:
            self.emit('capture')
        self.emit('draw_step', player=player)
        if self.present() and player == self.actor:
            if self.csvid == CLOCK:
                self.life = [life - (0 if self.prevented(seat) else self.doom) for seat, life in enumerate(self.life)]
            elif self.csvid == VAULT:
                self.life[player] -= 0 if self.prevented(player) else self.vault_tapped
        extra = int(self.present() and self.csvid in (NEKUSAR, VISIONS))
        self.draw_cards(player, 1 + extra)
        if before_receipts:
            before_receipts()
        self.receipts()
        if replay:
            self.emit('record_state')
            self.emit('restore')
            self.emit('draw_step', player=player)
            self.receipts()
            self.emit('assert_recorded_state')
        self.emit('turn', player=player)

    def visit(self, player, *, vault_pay=False, replay=False):
        self.untap(player)
        self.upkeep(player, vault_pay=vault_pay)
        self.draw(player, replay=replay)

    def remove(self):
        self.cast('removal', 'graveyard', target='source')
        self.cards['source']['zone'] = 'graveyard'
        self.receipts()

    def clock(self):
        if self.mode == 'upkeep_payment':
            self.clock_payment()
            return
        self.visit(self.foe)
        if self.mode == 'removed':
            self.remove()
        self.visit(self.actor, replay=self.mode == 'replay')
        self.mana('set_mana', self.actor, (4, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('can_activate', 'source', value=0)
        self.mana('set_mana', self.actor)
        self.visit(self.foe)
        self.visit(self.actor)

    def clock_payment(self):
        # Establish the life contrast with an actual damage spell so the
        # legacy AI availability gate cannot hide the direct activation.
        self.cast('clock_damage', 'graveyard', picked=self.foe)
        self.life[self.foe] -= 2
        self.receipts()
        self.visit(self.foe)
        self.visit(self.actor)
        self.untap(self.foe)
        self.upkeep(self.foe)

        # The actual opposing upkeep leaves its nonactive controller in
        # the native response actor slot. It matches this activation's
        # real source controller and payer; both pools can afford the fee.
        self.mana('set_mana', self.actor, (5, 0, 0, 0, 0, 0))
        self.mana('set_mana', self.foe, (6, 0, 0, 0, 0, 0))
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        self.emit('resolve')
        self.emit('assert_stack')
        self.mana('assert_mana', self.actor, (1, 0, 0, 0, 0, 0))
        self.mana('assert_mana', self.foe, (6, 0, 0, 0, 0, 0))
        self.doom -= 1
        self.choices()

        # The direct activation consumes this turn's draw preparation.
        # Clear diagnostic pools only after observing the actual payment.
        for player in (self.actor, self.foe):
            self.mana('set_mana', player)
        self.receipts()
        self.visit(self.actor)

    def vault(self):
        if self.mode != 'untapped':
            self.emit('activate', 'source', value=1)
            self.vault_tapped = 1
            self.observe('source', 'tapped', 1)
            self.emit('assert_stack')
            self.mana('assert_mana', self.actor, (3, 0, 0, 0, 0, 0))
            self.cast('mana_sink', 'battlefield', fund=False)
            self.receipts()
        self.visit(self.foe)
        self.visit(self.actor, vault_pay=self.mode == 'pay', replay=self.mode == 'tapped')
        if self.mode == 'pay':
            self.emit('activate', 'source', value=1)
            self.vault_tapped = 1
            self.observe('source', 'tapped', 1)
            self.emit('assert_mana', player=self.actor, color=0, value=3)
            self.emit('assert_stack')
            self.choices()

    def draw_damage(self):
        if self.mode == 'spell_draw':
            self.cast('own_draw', 'graveyard')
            self.draw_cards(self.actor, 2)
            self.receipts()
        self.visit(self.foe, replay=self.mode == 'replay')
        if self.mode == 'removed':
            self.remove()
        elif self.mode == 'spell_draw':
            self.cast('foe_draw', 'graveyard')
            self.draw_cards(self.foe, 2)
            self.receipts()
        self.visit(self.actor)

    def attack(self, *, block=False):
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if block:
            self.emit('block', 'blocker', 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')

    def asp(self):
        self.visit(self.foe)
        self.visit(self.actor)
        if self.mode == 'fog':
            self.cast('fog', 'graveyard')
        block = self.mode == 'blocked'
        self.attack(block=block)
        hit_player = self.mode not in ('fog', 'blocked')
        if hit_player:
            self.life[self.foe] -= 1
        if block:
            self.cards['source']['zone'] = 'graveyard'
            self.observe('blocker', 'damage', 1)
        self.receipts()
        if self.mode == 'removed_after_hit':
            self.cast('asp_removal', 'graveyard', target='source')
            self.cards['source']['zone'] = 'graveyard'
            self.receipts()
        self.untap(self.foe)
        self.upkeep(self.foe)
        paid = self.mode == 'paid'
        if paid and self.foe == 0:
            self.emit('choose_mana_source', 'asp_payment_land')

        def delayed_receipt():
            if paid:
                self.observe('asp_payment_land', 'tapped', 1)
            elif hit_player:
                self.life[self.foe] -= 1

        self.draw(self.foe, before_receipts=delayed_receipt)
        self.visit(self.actor)
        self.visit(self.foe)

    def masticore(self):
        self.visit(self.foe)
        self.untap(self.actor)
        pay = self.mode not in ('empty', 'upkeep_decline')
        if self.mode != 'empty':
            self.emit('choose_dialog', player=self.actor, value=int(not pay))
        if pay:
            if self.actor == 0:
                self.emit('choose_zone_card', player=0, value=self.rows['Swamp']['csvid'])
            else:
                self.emit('choose_dialog', player=1, value=0)
        self.emit('upkeep_step', player=self.actor)
        if pay:
            self.cards['discard_card']['zone'] = 'graveyard'
        else:
            self.cards['source']['zone'] = 'graveyard'
        self.receipts()
        fire = pay and self.mode != 'decline_shot'
        if fire:
            if self.actor == 0:
                self.emit('choose_trigger_source', 'source', player=0)
            self.emit('choose_target', 'shot_target', player=self.actor)

        def shot_receipt():
            if self.mode == 'foe_target':
                self.cards['shot_target']['zone'] = 'graveyard'
            else:
                self.observe('shot_target', 'damage', 3 if fire and self.mode in ('own_target', 'first_strike') else 0)

        self.draw(self.actor, before_receipts=shot_receipt if self.mode not in ('empty', 'upkeep_decline') else None)
        if self.mode == 'first_strike':
            self.attack(block=True)
            self.cards['blocker']['zone'] = 'graveyard'
            self.observe('source', 'damage', 0)
            self.receipts()

    def build(self):
        self.emit('turn', player=self.actor)
        self.mana('set_mana', self.actor)
        self.mana('set_mana', self.foe)
        self.cast('source', 'battlefield')
        self.receipts()
        if self.csvid == CLOCK:
            self.clock()
        elif self.csvid == VAULT:
            self.vault()
        elif self.csvid == ASP:
            self.asp()
        elif self.csvid == MASTICORE:
            self.masticore()
        else:
            self.draw_damage()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact draw-step primary CSVID required')
    cases = []
    for variant in selected:
        mode, player = variant.rsplit('_', 1)
        scenario = Board(csvid, int(player), mode, rows, oracle)
        scenario.build()
        cases.append(dict(name=f'DrawStepDamageAndUpkeepRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=2528, setup=scenario.setup, steps=scenario.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],
                                       untested_components=list(GAPS.get(csvid, ())))))
    return cases
