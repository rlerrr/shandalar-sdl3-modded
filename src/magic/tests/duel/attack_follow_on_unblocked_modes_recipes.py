"""Actual blocker declarations, optional unblocked effects, and damage receipts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'attack_follow_on_unblocked_modes'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'AttackFollowOnUnblockedModesRecipe'
SEED = 3062
OPHIDIAN, RATS, DANCER, MASTER = 3020, 5780, 2948, 13170
WURM, ZEALOT, KEEPER, PIRATES, HIPPO, WILDFIRE = 5781, 1777, 2302, 2524, 2758, 17256
ORIGINAL = (OPHIDIAN, RATS, DANCER, MASTER)
MODES = {
    OPHIDIAN: ('take', 'decline', 'blocked', 'repeat', 'other_attacks'),
    RATS: ('foe_pay', 'self_pay', 'decline', 'attack_blocked', 'block_foe_pay', 'block_self_pay', 'block_decline'),
    DANCER: ('take', 'borrow_owner', 'shroud', 'wrong_grave', 'blocked', 'decline'),
    MASTER: ('unblocked', 'at_one', 'master_first', 'other_first', 'first_strike', 'walker'),
    WURM: ('foe_pay', 'self_pay', 'decline', 'short', 'block_foe_pay', 'block_self_pay'),
    ZEALOT: ('kill', 'own', 'nonlethal', 'decline', 'blocked', 'other_attacks'),
    KEEPER: ('unblocked', 'blocked', 'other_attacks'),
    PIRATES: ('artifact', 'borrow_owner', 'decline', 'none', 'blocked'),
    HIPPO: ('paid', 'no_lands', 'blocked', 'decline'),
    WILDFIRE: ('free_instant', 'free_sorcery', 'decline', 'blocked_afflict', 'nonspell'),
}
ZONE = ('hand', 'battlefield', 'library', 'graveyard', 'exile')
ZERO = (0,) * 6



@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal unblocked combat rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned unblocked combat source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned unblocked combat declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported unblocked combat variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed unblocked combat fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZONE} for p in (0, 1)}
        self.life = [20, 20]
        self.pools = [ZERO, ZERO]
        self.add('source', self.name, owner, 'hand')
        if self.id == OPHIDIAN:
            self.add('first_draw', 'Island', owner, 'library')
            self.add('second_draw', 'Giant Growth', owner, 'library')
            self.add('foreign_deck', 'Mountain', self.foe, 'library')
            if mode == 'other_attacks':
                self.add('other_attacker', 'Goblin Piker', owner, 'battlefield')
            if mode == 'blocked':
                self.add('blocker', 'Grizzly Bears', self.foe, 'battlefield')
        elif self.id in (RATS, WURM):
            paying = owner if mode in ('self_pay', 'block_self_pay') else self.foe
            self.add('last_grave', 'Lightning Bolt', paying, 'graveyard')
            self.add('payment', 'Forest', paying, 'graveyard')
            self.payments = ['payment']
            if self.id == WURM and mode != 'short':
                for index, name in enumerate(('Mountain', 'Hill Giant')):
                    ref = f'payment_{index}'
                    self.add(ref, name, paying, 'graveyard')
                    self.payments.append(ref)
            if mode.startswith('block_') or mode == 'attack_blocked':
                self.add('adversary', 'Grizzly Bears' if self.id == RATS else 'Rumbling Baloth', self.foe, 'battlefield')
        elif self.id == DANCER:
            self.add('own_creature', 'Walking Corpse', owner, 'graveyard')
            if mode != 'wrong_grave':
                self.add('older_creature', 'Hill Giant', self.foe, 'graveyard')
                self.add('recipient', 'Lone Missionary', self.foe, 'graveyard')
            # The top card overall does not stop selection of top creature.
            self.add('foreign_spell', 'Unsummon', self.foe, 'graveyard')
            if mode == 'shroud':
                self.add('shroud', 'Silent Gravestone', self.foe, 'battlefield')
            if mode == 'borrow_owner':
                self.add('bounce', 'Unsummon', owner, 'hand')
            if mode == 'blocked':
                self.add('blocker', 'Horned Turtle', self.foe, 'battlefield')
        elif self.id == MASTER:
            if mode in ('master_first', 'other_first'):
                self.add('other_attacker', 'Goblin Piker', owner, 'battlefield')
            if mode == 'first_strike':
                self.add('blocker', 'Spined Wurm', self.foe, 'battlefield')
            if mode == 'walker':
                self.add('walker', 'Jace Beleren', self.foe, 'hand')
        else:
            self.stage_follower()

    def emit(self, op, ref='', *, prop='', **values):
        step = dict(op=op, **values)
        if ref:
            step['ref'] = ref
        if prop:
            step['property'] = prop
        self.steps.append(step)

    def add(self, ref, name, player, zone):
        self.setup.append(dict(ref=ref, name=name, player=player, zone=zone))
        self.cards[ref] = name, player, zone
        self.zones[player][zone].append(name)

    def move(self, ref, zone, *, seat=None):
        name, player, old = self.cards[ref]
        new_player = player if seat is None else seat
        self.zones[player][old].remove(name)
        self.zones[new_player][zone].append(name)
        self.cards[ref] = name, new_player, zone

    def prop(self, ref, key, value):
        self.emit('assert_card', ref, prop=key, value=value)

    def mana(self, player, amounts=ZERO, *, write=False):
        for color, amount in enumerate(amounts):
            self.emit('set_mana' if write else 'assert_mana', player=player, value=amount, color=color)

    def cast(self, ref, destination, target=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]), write=True)
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'in_hand', 0)
        self.mana(player)
        self.emit('assert_unused_choices', value=0)
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')
        self.move(ref, destination)

    def stats(self, ref, *, damage=None):
        name, _, zone = self.cards[ref]
        assert zone == 'battlefield'
        self.prop(ref, 'in_play', 1)
        self.prop(ref, 'csvid', self.rows[name]['csvid'])
        for field in ('power', 'toughness'):
            self.prop(ref, field, int(self.oracle[name][field]))
        if damage is not None:
            self.prop(ref, 'damage', damage)

    def next_main(self, seat):
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        self.emit('turn', player=seat)

    def ready_source(self):
        self.next_main(self.foe)
        self.next_main(self.owner)
        self.prop('source', 'sick', 0)

    def attack(self, ref, *, accepted=True, walker=False):
        if walker:
            self.emit('choose_dialog', player=self.owner, value=1)
        self.emit('attack_with_tax' if walker else 'attack', ref, value=int(accepted))
        self.prop(ref, 'attacking', int(accepted))
        self.prop(ref, 'tapped', int(accepted))

    def finish_combat(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')

    def unblocked_option(self, *, available, accept):
        self.emit('can_activate', 'source', value=int(available))
        if accept:
            self.emit('activate', 'source', value=1)
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.emit('assert_stack', value=0)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=0)
        elif not available:
            self.emit('activate', 'source', value=0)
            self.emit('assert_stack', value=0)

    def ophidian(self):
        self.ready_source()
        other = self.mode == 'other_attacks'
        blocked = self.mode == 'blocked'
        attacker = 'other_attacker' if other else 'source'
        count = 2 if self.mode == 'repeat' else 1
        for swing in range(count):
            if swing:
                self.ready_source()
            self.emit('can_activate', 'source', value=0)
            self.emit('begin_attackers')
            self.attack(attacker)
            self.emit('finish_attackers')
            self.emit('can_activate', 'source', value=0)
            self.emit('begin_blockers')
            if blocked:
                self.emit('block', 'blocker', other='source', value=1)
            self.emit('advance_combat')
            self.prop(attacker, 'blocked', int(blocked))
            take = (self.mode in ('take', 'repeat')) and not swing
            self.unblocked_option(available=not (other or blocked), accept=take)
            if take:
                self.move('first_draw', 'hand')
            self.receipts()
            self.finish_combat()
            if blocked:
                self.stats('source', damage=2)
                self.stats('blocker', damage=1)
            elif not take:
                self.life[self.foe] -= 2 if other else 1

    def rats(self):
        defending = self.mode.startswith('block_')
        paying = self.owner if self.mode in ('self_pay', 'block_self_pay') else self.foe
        pay = self.mode not in ('decline', 'block_decline', 'short')
        blocked = defending or self.mode == 'attack_blocked'
        attacker = 'adversary' if defending else 'source'
        blocker = 'source' if defending else 'adversary'
        if defending:
            self.next_main(self.foe)
        else:
            self.ready_source()
        def inputs():
            if self.mode == 'short':
                return
            self.emit('choose_dialog', player=paying, value=int(not pay))
            if pay:
                for ref in self.payments:
                    cid = self.rows[self.cards[ref][0]]['csvid']
                    self.emit('choose_zone_card', player=paying, value=cid)
        self.emit('begin_attackers')
        self.attack(attacker)
        if not defending:
            inputs()
        self.emit('finish_attackers')
        if not defending:
            self.emit('assert_unused_choices', value=0)
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', blocker, other=attacker, value=1)
        if defending:
            inputs()
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)
        if pay:
            for ref in self.payments:
                self.move(ref, 'exile')
        self.receipts()
        self.finish_combat()
        if blocked:
            if self.id == RATS:
                self.move('source', 'graveyard')
            else:
                self.stats('source', damage=4)
            if pay:
                self.stats('adversary', damage=0)
            else:
                self.move('adversary', 'graveyard')
        elif not pay:
            self.life[self.foe] -= int(self.oracle[self.name]['power'])

    def dancer(self):
        self.ready_source()
        blocked = self.mode == 'blocked'
        self.emit('can_activate', 'source', value=0)
        self.emit('begin_attackers')
        self.attack('source')
        self.emit('finish_attackers')
        self.emit('can_activate', 'source', value=0)
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', 'blocker', other='source', value=1)
        self.emit('advance_combat')
        self.prop('source', 'blocked', int(blocked))
        take = self.mode in ('take', 'borrow_owner', 'shroud')
        self.unblocked_option(available=not blocked and self.mode != 'wrong_grave', accept=take)
        if take:
            self.move('recipient', 'battlefield', seat=self.owner)
            self.life[self.owner] += 4
        self.receipts()
        self.finish_combat()
        if blocked:
            self.stats('source', damage=1)
            self.stats('blocker', damage=2)
        elif not take:
            self.life[self.foe] -= 2
        if self.mode == 'borrow_owner':
            cid = self.rows['Lone Missionary']['csvid']
            self.emit('bind_permanent_copy', 'raised', player=self.owner, index=0, value=cid)
            self.cards['raised'] = self.cards['recipient']
            self.cast('bounce', 'graveyard', 'raised')
            self.move('raised', 'hand', seat=self.foe)

    def master(self):
        walker = self.mode == 'walker'
        if walker:
            self.emit('turn', player=self.foe)
            self.cast('walker', 'battlefield')
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=3)
            self.emit('turn', player=self.owner)
        if self.mode == 'at_one':
            self.life[self.foe] = 1
            self.emit('set_life', player=self.foe, value=1)
        self.ready_source()
        blocked = self.mode == 'first_strike'
        others = self.mode == 'other_first'
        self.emit('begin_attackers')
        if others:
            self.attack('other_attacker')
            self.attack('source', accepted=False)
        else:
            self.attack('source', walker=walker)
            if self.mode == 'master_first':
                self.attack('other_attacker', accepted=False)
        self.emit('assert_unused_choices', value=0)
        self.emit('finish_attackers')
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        self.emit('begin_blockers')
        if blocked:
            self.emit('block', 'blocker', other='source', value=1)
        self.emit('advance_combat')
        if not (blocked or walker or others):
            self.life[self.foe] = 1
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        if walker:
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=3)
        self.finish_combat()
        if blocked:
            self.move('blocker', 'graveyard')
            self.stats('source', damage=0)
        elif walker:
            self.emit('assert_counter', 'walker', counter_kind='loyalty', value=2)
        elif others:
            self.life[self.foe] -= 2

    def stage_follower(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        if self.id in (ZEALOT, KEEPER):
            if mode == 'other_attacks':
                self.add('other_attacker', 'Goblin Piker', owner, 'battlefield')
            elif mode == 'blocked':
                name = 'Horned Turtle' if self.id == ZEALOT else 'Grizzly Bears'
                self.add('blocker', name, foe, 'battlefield')
            elif self.id == ZEALOT:
                name = 'Horned Turtle' if mode == 'nonlethal' else 'Hill Giant'
                recipient = owner if mode == 'own' else foe
                self.add('recipient', name, recipient, 'battlefield')
        elif self.id == PIRATES:
            self.add('own_artifact', 'Darksteel Relic', owner, 'battlefield')
            if mode != 'none':
                target = 'Ornithopter' if mode == 'borrow_owner' else 'Mind Stone'
                self.add('recipient', target, foe, 'battlefield')
            if mode == 'blocked':
                self.add('blocker', 'Horned Turtle', foe, 'battlefield')
            if mode == 'borrow_owner':
                self.add('bounce', 'Unsummon', owner, 'hand')
            if mode == 'artifact':
                self.add('removal', 'Murder', foe, 'hand')
        elif self.id == HIPPO:
            if mode != 'no_lands':
                self.add('blue_land', 'Island', foe, 'battlefield')
                self.add('red_land', 'Mountain', foe, 'battlefield')
                self.add('spent_land', 'Forest', foe, 'battlefield')
            if mode == 'blocked':
                self.add('blocker', 'Horned Turtle', foe, 'battlefield')
        else:
            self.add('first_draw', 'Island', owner, 'library')
            self.add('second_draw', 'Mountain', owner, 'library')
            self.add('foreign_draw', 'Forest', foe, 'library')
            spell = {'free_sorcery': 'Divination', 'nonspell': 'Mind Stone'}.get(mode, 'Reach Through Mists')
            self.add('free_spell', spell, owner, 'hand')
            if mode == 'blocked_afflict':
                self.add('blocker', 'Grizzly Bears', foe, 'battlefield')

    def start_follower_combat(self, *, attacker='source', blocker=None, decisions=None):
        self.emit('begin_attackers')
        self.attack(attacker)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block', blocker, other=attacker, value=1)
        if decisions:
            decisions()
        self.emit('advance_combat')
        self.emit('assert_unused_choices', value=0)
        self.prop(attacker, 'blocked', int(blocker is not None))

    def zealot(self):
        self.ready_source()
        other = self.mode == 'other_attacks'
        blocked = self.mode == 'blocked'
        selected = self.mode in ('kill', 'own', 'nonlethal')
        if self.mode == 'nonlethal':
            # The actual AI uses an avoid-lethal targeting heuristic. A two-life
            # opponent makes choosing a nonlethal creature a real offered option.
            self.life[self.foe] = 2
            self.emit('set_life', player=self.foe, value=2)
        def inputs():
            if 'recipient' in self.cards:
                self.emit('choose_dialog', player=self.owner, value=int(not selected))
                if selected:
                    self.emit('choose_target', 'recipient', player=self.owner)
        self.start_follower_combat(attacker='other_attacker' if other else 'source',
                                   blocker='blocker' if blocked else None, decisions=inputs)
        self.finish_combat()
        if blocked:
            self.stats('source', damage=1)
            self.stats('blocker', damage=2)
        elif selected:
            if self.mode == 'nonlethal':
                self.stats('recipient', damage=3)
            else:
                self.move('recipient', 'graveyard')
        else:
            self.life[self.foe] -= 2

    def keeper(self):
        self.ready_source()
        other = self.mode == 'other_attacks'
        blocked = self.mode == 'blocked'
        self.start_follower_combat(attacker='other_attacker' if other else 'source',
                                   blocker='blocker' if blocked else None)
        if not (other or blocked):
            self.life[self.foe] -= 2
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        self.finish_combat()
        if blocked:
            self.move('blocker', 'graveyard')
            self.stats('source', damage=2)
        elif other:
            self.life[self.foe] -= 2

    def pirates(self):
        self.ready_source()
        blocked = self.mode == 'blocked'
        take = self.mode in ('artifact', 'borrow_owner')
        def inputs():
            if 'recipient' in self.cards and not blocked:
                self.emit('choose_target' if take else 'choose_cancel_target',
                          'recipient' if take else '', player=self.owner)
        self.start_follower_combat(blocker='blocker' if blocked else None, decisions=inputs)
        if take:
            self.move('recipient', 'battlefield', seat=self.owner)
        self.receipts()
        self.finish_combat()
        if blocked:
            self.stats('source', damage=1)
            self.stats('blocker', damage=2)
        elif not take:
            self.life[self.foe] -= 2
        if self.mode == 'borrow_owner':
            name, controller, zone = self.cards['recipient']
            self.emit('bind_permanent_copy', 'stolen', player=controller, index=0, value=self.rows[name]['csvid'])
            self.cards['stolen'] = name, controller, zone
            self.cast('bounce', 'graveyard', 'stolen')
            self.move('stolen', 'hand', seat=self.foe)
        elif self.mode == 'artifact':
            self.cast('removal', 'graveyard', 'source')
            self.move('source', 'graveyard')
            self.emit('cleanup')
            self.emit('refresh')

    def hippo(self):
        self.ready_source()
        if self.mode == 'paid':
            self.emit('activate', 'spent_land', value=1)
            self.prop('spent_land', 'tapped', 1)
            self.mana(self.foe, (0, 0, 0, 1, 0, 0))
        blocked = self.mode == 'blocked'
        used = self.mode in ('paid', 'no_lands')
        if used:
            self.emit('can_activate', 'source', value=0)
        self.start_follower_combat(blocker='blocker' if blocked else None)
        self.unblocked_option(available=not blocked, accept=used)
        if self.mode != 'no_lands':
            for ref in ('blue_land', 'red_land', 'spent_land'):
                self.prop(ref, 'tapped', int(self.mode == 'paid'))
        self.mana(self.foe)
        self.mana(self.owner)
        self.emit('combat_damage')
        self.emit('resolve')
        if blocked:
            self.stats('source', damage=1)
            self.stats('blocker', damage=2)
        elif not used:
            self.life[self.foe] -= 2
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        self.mana(self.owner)
        self.emit('end_combat_step', value=0)
        self.emit('refresh')
        if self.mode == 'paid':
            self.pools[self.owner] = (3, 0, 0, 0, 0, 0)

    def wildfire(self):
        self.ready_source()
        blocked = self.mode == 'blocked_afflict'
        free = self.mode in ('free_instant', 'free_sorcery')
        def inputs():
            if self.mode == 'decline':
                self.emit('choose_cancel_target', player=self.owner)
            elif free:
                self.emit('choose_target', 'free_spell', player=self.owner)
        self.start_follower_combat(blocker='blocker' if blocked else None, decisions=inputs)
        self.emit('resolve')
        self.emit('refresh')
        if blocked:
            self.life[self.foe] -= 4
        self.emit('assert_life', player=self.foe, value=self.life[self.foe])
        if free:
            self.move('free_spell', 'graveyard')
            self.move('first_draw', 'hand')
            if self.mode == 'free_sorcery':
                self.move('second_draw', 'hand')
        self.receipts()
        self.finish_combat()
        if blocked:
            self.stats('source', damage=2)
            self.stats('blocker', damage=1)
        else:
            self.life[self.foe] -= 1


    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        self.cast('source', 'battlefield')
        self.stats('source')
        cases = {OPHIDIAN: self.ophidian, RATS: self.rats, DANCER: self.dancer, MASTER: self.master,
                 WURM: self.rats, ZEALOT: self.zealot, KEEPER: self.keeper, PIRATES: self.pirates,
                 HIPPO: self.hippo, WILDFIRE: self.wildfire}
        cases[self.id]()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.mana(player, self.pools[player])
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=0)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('unblocked combat source CSVID changed')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        gaps = (
            ['The existing source offers its optional effect via an after-blocker activation. The fixture observes that path.']
            if csvid in (OPHIDIAN, DANCER, HIPPO) else []
        )
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
