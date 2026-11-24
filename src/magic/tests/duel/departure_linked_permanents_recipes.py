"""Linked battlefield departures with actual ownership and choice receipts."""

from functools import lru_cache
import json
from pathlib import Path

from entry_follow_on_damage_and_tap_recipes import CombatBoard, ZERO, ZONES
from library_search_recipes import wave6_exact as exact
from recipes import COLORS, cost_from_oracle, step


FAMILY = 'departure_linked_permanents'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
SEED = 3596
AURIFICATION, THIEF, SHELL, TOMB = 6056, 4095, 11224, 54
DUAL, NEXUS, TIDE, WAVE, ACID, NETS, ARTISAN = 4818, 4732, 4733, 4734, 8830, 3582, 18570
EGG, VAULT, LICH, NEFARIOUS, ALGAE, WAR_BEAST, NEVERMAKER = 7131, 700, 141, 5635, 1159, 2332, 9396

MODES = {
    AURIFICATION: ('defender', 'wall', 'leaves', 'noncombat', 'spell'),
    THIEF: ('all', 'attachment', 'bounce', 'exile', 'flying'),
    SHELL: ('creature_order', 'noncreature', 'bounce', 'empty'),
    TOMB: ('restoration', 'bounce', 'two_upkeeps', 'wrong_time', 'only_swamp'),
    DUAL: ('copy_foreign', 'original_leaves', 'source_leaves'),
    NEXUS: ('foreign_hand', 'fading'),
    TIDE: ('both_owners', 'fading'),
    WAVE: ('both_owners', 'fading'),
    ACID: ('bounce', 'vanish', 'own_target'),
    NETS: ('release', 'unblocked', 'defender'),
    ARTISAN: ('delayed_own', 'delayed_foreign', 'noncreature'),
    EGG: ('creature', 'noncreature', 'decline'),
    VAULT: ('return', 'graveyard', 'empty'),
    LICH: ('draw', 'damage', 'graveyard'),
    NEFARIOUS: ('draw', 'replace_damage', 'short_grave', 'leaves'),
    ALGAE: ('tap', 'non_swamp', 'exile'),
    WAR_BEAST: ('bounce', 'destroy', 'no_land'),
    NEVERMAKER: ('evoke', 'bounce', 'flying', 'no_target'),
}


def label(csvid, mode, actor):
    return f'DepartureLinkedPermanentsRecipe_{csvid}_{mode}_{actor}'


class Board(CombatBoard):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1 - actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.uses, self.changed_lands = set(), set()
        self.add('source', record['name'], actor, 'hand')
        getattr(self, {AURIFICATION: 'build_aurification', THIEF: 'build_thief',
                       SHELL: 'build_shell', TOMB: 'build_tomb'}[csvid])()

    def add(self, ref, name, player, zone, *, tapped=False):
        self.uses.add(name)
        super().add(ref, name, player, zone, tapped=tapped)

    def build_aurification(self):
        name = 'Prodigal Sorcerer' if self.mode == 'noncombat' else 'Goblin Tinkerer'
        self.add('raider', name, self.foe, 'battlefield')
        if self.mode == 'wall':
            self.add('tunnel', 'Tunnel', self.actor, 'hand')
        elif self.mode == 'leaves':
            self.add('other', 'Prodigal Sorcerer', self.foe, 'battlefield')
            self.add('removal', 'Disenchant', self.actor, 'hand')
        elif self.mode == 'spell':
            self.add('shock', 'Shock', self.foe, 'hand')

    def build_thief(self):
        self.add('guard', 'Goblin Tinkerer', self.foe, 'battlefield')
        self.add('ally', 'Prodigal Sorcerer', self.actor, 'battlefield')
        if self.mode == 'attachment':
            self.add('aura', 'Unholy Strength', self.foe, 'hand')
        else:
            self.add('anthem', 'Glorious Anthem', self.foe, 'battlefield')
        if self.mode == 'all':
            self.add('levitation', 'Levitation', self.foe, 'battlefield')
            self.add('artifact', "Traveler's Amulet", self.foe, 'battlefield')
        removal = {'all': 'Murder', 'attachment': 'Murder', 'bounce': 'Unsummon',
                   'exile': 'Swords to Plowshares'}.get(self.mode)
        if removal:
            self.add('removal', removal, self.actor, 'hand')

    def build_shell(self):
        if self.mode in ('creature_order', 'bounce'):
            names = ('Forest', 'Goblin Tinkerer', 'Island', 'Mountain', 'Plains')
            self.imprinted = 'library_1'
            self.bottom = ('library_3', 'library_0', 'library_2')
            self.library = ['library_4', *self.bottom]
        elif self.mode == 'noncreature':
            names = ('Forest', 'Island')
            self.imprinted = 'library_0'
            self.bottom = ('library_1',)
            self.library = list(self.bottom)
        else:
            names = ()
            self.imprinted, self.bottom, self.library = None, (), []
        for index, name in enumerate(names):
            self.add(f'library_{index}', name, self.actor, 'library')
        self.add('removal', 'Unsummon' if self.mode == 'bounce' else 'Murder', self.actor, 'hand')

    def build_tomb(self):
        self.add('land', 'Swamp' if self.mode == 'only_swamp' else 'Forest', self.foe, 'battlefield')
        if self.mode == 'two_upkeeps':
            self.add('second', 'Island', self.foe, 'battlefield')
        if self.mode in ('restoration', 'two_upkeeps', 'bounce'):
            self.add('removal', 'Disperse' if self.mode == 'bounce' else 'Disenchant', self.actor, 'hand')

    def receipt(self):
        if not self.changed_lands:
            super().receipt()
            return
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.pools()
        for player in (0, 1):
            for zone, code in ZONES.items():
                cards = {ref: card['name'] for ref, card in self.cards.items()
                         if card['zone'] == zone and card['player'] == player}
                self.emit('assert_zone', player=player, zone=code, value=len(cards))
                names = [name for ref, name in cards.items() if ref not in self.changed_lands]
                for name in sorted(set(names)):
                    self.emit('assert_zone_multiplicity', player=player, zone=code,
                              index=self.rows[name]['csvid'], value=names.count(name))

    def gold(self, ref, amount):
        self.emit('assert_counter', ref, prop='gold', value=amount)

    def opposing_attack(self, refs, *, permitted=True):
        self.emit('begin_attackers')
        for ref in refs:
            self.prop(ref, 'tapped', 0)
            self.emit('attack', ref, value=int(permitted))
            self.prop(ref, 'attacking', int(permitted))
        self.emit('finish_attackers')
        if not permitted:
            return
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.life[self.actor] -= sum(self.stats(ref)[0] for ref in refs)

    def aurification(self):
        self.cast('source', destination='battlefield')
        self.gold('raider', 0)
        self.emit('advance_turn')
        if self.mode == 'noncombat':
            self.emit('require_can_activate', 'raider', value=1)
            self.emit('choose_player', player=self.foe, value=self.actor)
            self.emit('activate', 'raider', value=1)
            self.prop('raider', 'tapped', 1)
            self.emit('assert_stack', value=1)
            self.emit('resolve')
            self.life[self.actor] -= 1
            self.gold('raider', 1)
            return
        if self.mode == 'spell':
            self.fund(self.foe, cost_from_oracle(self.oracle['Shock']))
            self.emit('choose_player', player=self.foe, value=self.actor)
            self.emit('cast', 'shock', value=1)
            self.emit('assert_stack', value=1)
            self.wallet[self.foe] = ZERO
            self.pools()
            self.emit('resolve')
            self.move('shock', 'graveyard')
            self.life[self.actor] -= 2
            self.gold('raider', 0)
            return
        refs = ('raider', 'other') if self.mode == 'leaves' else ('raider',)
        self.opposing_attack(refs)
        for ref in refs:
            self.gold(ref, 1)
        self.receipt()
        self.emit('advance_turn')
        if self.mode == 'wall':
            self.cast('tunnel', target='raider')
            self.move('raider', 'graveyard')
            return
        if self.mode == 'leaves':
            self.cast('removal', target='source')
            self.move('source', 'graveyard')
            for ref in refs:
                self.gold(ref, 0)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('turn', player=self.foe)
        self.opposing_attack(refs, permitted=self.mode == 'leaves')

    def thief(self):
        if self.mode == 'attachment':
            self.cast('aura', target='guard', destination='battlefield')
            self.size('guard', (3, 3))
            self.emit('advance_turn')
        self.cast('source', destination='battlefield')
        self.size('source')
        if self.mode == 'flying':
            self.round_trip_turn()
            self.battle('source', attempts=(('guard', False),))
            return
        if self.mode == 'all':
            self.size('ally', (1, 1))
            self.size('guard', (2, 3))
        self.cast('removal', target='source')
        self.move('source', {'bounce': 'hand', 'exile': 'exile'}.get(self.mode, 'graveyard'))
        if self.mode == 'attachment':
            self.cards['aura']['player'] = self.actor
            self.size('guard', (3, 3))
            self.size('ally', (1, 1))
        elif self.mode == 'all':
            for ref in ('anthem', 'levitation'):
                self.cards[ref]['player'] = self.actor
            self.size('ally', (2, 2))
            self.size('guard', (1, 2))
            self.round_trip_turn()
            self.battle('ally', attempts=(('guard', False),), stats=(2, 2))
        else:
            if self.mode == 'exile':
                self.life[self.actor] += 2
            self.size('ally', (1, 1))
            self.size('guard', (2, 3))

    def zone_choice(self, ref):
        self.emit('choose_zone_card', player=self.actor, value=self.rows[self.cards[ref]['name']]['csvid'])

    def shell(self):
        self.pending('source')
        if self.imprinted:
            if self.actor == 1 and self.mode == 'noncreature':
                self.emit('observe_search_filters', value=1)
                self.emit('choose_zone', player=self.actor, value=-1)
            self.zone_choice(self.imprinted)
            for ref in self.bottom[:-1]:
                self.zone_choice(ref)
        self.emit('resolve')
        self.move('source', 'battlefield')
        self.size('source')
        if self.imprinted:
            self.move(self.imprinted, 'exile')
        for index, ref in enumerate(self.library):
            self.emit('assert_zone_card', player=self.actor, zone=ZONES['library'],
                      index=index, value=self.rows[self.cards[ref]['name']]['csvid'])
        self.receipt()

        self.fund(self.actor, cost_from_oracle(self.oracle[self.cards['removal']['name']]))
        self.emit('choose_target', 'source', player=self.actor)
        self.emit('cast', 'removal', value=1)
        self.emit('assert_stack', value=1)
        self.wallet[self.actor] = ZERO
        self.pools()
        bounced = self.mode == 'bounce'
        if self.imprinted and not bounced:
            self.emit('choose_zone', player=self.actor, value=0)
        self.emit('resolve')
        if self.imprinted and not bounced:
            self.emit('assert_unused_choices', value=0)
        self.move('removal', 'graveyard')
        self.move('source', 'hand' if bounced else 'graveyard')
        if self.mode == 'creature_order':
            self.move(self.imprinted, 'battlefield')

    def owner_upkeep(self, target=None):
        self.emit('advance_turn')
        self.emit('untap_step', player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step', player=self.actor)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('upkeep_step', player=self.actor)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        if target:
            self.changed_lands.remove(target)

    def mire(self, target):
        self.fund(self.actor, (2, 0, 0, 0, 0, 0))
        self.emit('require_can_activate', 'source', value=1)
        self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.prop('source', 'tapped', 1)
        self.emit('assert_stack', value=1)
        self.wallet[self.actor] = ZERO
        self.pools()
        self.emit('resolve')
        self.changed_lands.add(target)
        self.receipt()

    def land_mana(self, ref, color):
        self.prop(ref, 'tapped', 0)
        self.emit('activate', ref, value=1)
        self.prop(ref, 'tapped', 1)
        self.emit('assert_stack', value=0)
        pool = [0] * 6
        pool[color] = 1
        self.wallet[self.foe] = tuple(pool)
        self.pools()
        self.fund(self.foe, ZERO)

    def tomb(self):
        self.cast('source', destination='battlefield')
        if self.mode == 'wrong_time':
            self.fund(self.actor, (2, 0, 0, 0, 0, 0))
            self.emit('can_activate', 'source', value=0)
            self.prop('source', 'tapped', 0)
            return
        self.owner_upkeep()
        if self.mode == 'only_swamp':
            self.fund(self.actor, (2, 0, 0, 0, 0, 0))
            self.emit('can_activate', 'source', value=0)
            return
        self.mire('land')
        self.land_mana('land', COLORS['B'])
        if self.mode == 'two_upkeeps':
            self.owner_upkeep()
            self.mire('second')
            self.land_mana('second', COLORS['B'])
        self.cast('removal', target='source')
        bounced = self.mode == 'bounce'
        self.move('source', 'hand' if bounced else 'graveyard')
        self.owner_upkeep(None if bounced else 'land')
        self.land_mana('land', COLORS['B'] if bounced else COLORS['G'])
        if self.mode == 'two_upkeeps':
            self.land_mana('second', COLORS['B'])
            self.owner_upkeep('second')
            self.land_mana('second', COLORS['U'])

    def battle(self, attacker, *, attempts=(), blocker=None, stats=None, vigilance=False):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', int(not vigilance))
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('begin_blockers')
        for candidate, allowed in attempts:
            self.emit('block', candidate, attacker, value=int(allowed))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocker is not None))
        if blocker is not None:
            self.prop(blocker, 'blocking_count', 1)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power, tough = stats or self.stats(attacker)
        if blocker is None:
            self.life[self.foe] -= power
        else:
            opposing_power, opposing_tough = self.stats(blocker)
            for ref, damage, endurance in ((attacker, opposing_power, tough), (blocker, power, opposing_tough)):
                if damage >= endurance:
                    self.move(ref, 'graveyard')
                else:
                    self.prop(ref, 'damage', damage)
        self.receipt()

    def play(self):
        starts = self.foe if self.csvid == THIEF and self.mode == 'attachment' else self.actor
        self.emit('turn', player=starts)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.fund(player, ZERO)
        action = {AURIFICATION: self.aurification, THIEF: self.thief, SHELL: self.shell, TOMB: self.tomb}
        action[self.csvid]()
        record_start = len(self.steps)
        self.receipt()
        if self.csvid == SHELL and self.mode in ('creature_order', 'noncreature'):
            assert self.steps[record_start]['op'] == 'assert_choices_consumed'
            del self.steps[record_start]



class LinkedBoard(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid, mode, actor, 1-actor
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.wallet, self.life = [ZERO, ZERO], [20, 20]
        self.uses, self.changed_lands = set(), set()
        self.add('source', record['name'], actor, 'hand')
        self.action = {
            DUAL: self.dual, NEXUS: self.parallax, TIDE: self.parallax, WAVE: self.parallax,
            ACID: self.acid, NETS: self.nets, ARTISAN: self.artisan, EGG: self.egg,
            VAULT: self.vault, LICH: self.lich, NEFARIOUS: self.lich, ALGAE: self.algae,
            WAR_BEAST: self.war_beast, NEVERMAKER: self.nevermaker,
        }[csvid]

    def begin_main(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.fund(player, ZERO)

    def hand(self, ref, name, owner=None):
        self.add(ref, name, self.actor if owner is None else owner, 'hand')

    def field(self, ref, name, owner=None, *, tapped=False):
        self.add(ref, name, self.actor if owner is None else owner, 'battlefield', tapped=tapped)

    def named_zone(self, ref):
        entry = self.cards[ref]
        self.emit('choose_zone_card', player=entry['player'], value=self.rows[entry['name']]['csvid'])

    def zone_id(self, ref, zone, amount):
        card = self.cards[ref]
        self.emit('assert_zone_multiplicity', player=card['player'], zone=ZONES[zone],
                  index=self.rows[card['name']]['csvid'], value=amount)

    def pend_spell(self, ref, *, target=None, target_player=None, mode=None):
        card = self.cards[ref]
        player = card['player']
        self.fund(player, cost_from_oracle(self.oracle[card['name']]))
        if mode is not None:
            self.emit('choose_dialog', player=player, value=mode)
        if target_player is not None:
            self.emit('choose_player', player=player, value=target_player)
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[player] = ZERO
        self.pools()

    def settle(self, ref, destination='graveyard', *, input_receipt='assert_choices_consumed'):
        self.emit('resolve')
        self.emit(input_receipt, value=0)
        self.emit('assert_stack', value=0)
        self.move(ref, destination)

    def cast_exit_spell(self, name, *, target='source', destination='graveyard'):
        self.hand('exit', name)
        self.cast('exit', target=target)
        self.move(target, destination)

    def counter(self, kind, amount):
        self.emit('assert_counter', 'source', prop=kind, value=amount)

    def activate_pending(self, fee, *, target=None, target_player=None, mode=None, tap=False):
        self.fund(self.actor, fee)
        if mode is not None:
            self.emit('choose_dialog', player=self.actor, value=mode)
        if target_player is not None:
            self.emit('choose_player', player=self.actor, value=target_player)
        if target:
            self.emit('choose_target', target, player=self.actor)
        self.emit('activate', 'source', value=1)
        self.emit('assert_stack', value=1)
        if tap:
            self.prop('source', 'tapped', 1)
        self.wallet[self.actor] = ZERO
        self.pools()

    def token(self, ref, name, owner):
        self.cards[ref] = dict(name=name, player=owner, zone='battlefield')

    def dual(self):
        opposing = self.mode == 'copy_foreign'
        player = self.foe if opposing else self.actor
        self.hand('original', 'Goblin Tinkerer', player)
        self.hand('other', 'Prodigal Sorcerer', player)
        self.hand('exit', 'Disenchant' if self.mode == 'source_leaves' else 'Unsummon', player)
        self.cast('source', destination='battlefield')
        if opposing:
            self.emit('advance_turn')
        for ref in ('original', 'other'):
            entry = self.cards[ref]
            self.cast(ref, destination='battlefield')
            self.token(ref + '_copy', entry['name'], player)
            self.zone_id(ref, 'battlefield', 2)
            for prop, value in zip(('power', 'toughness'), self.stats(ref)):
                self.emit('assert_matching_permanents', player=player,
                          index=self.rows[entry['name']]['csvid'], prop=prop, value=value)
        source_leaves = self.mode == 'source_leaves'
        self.cast('exit', target='source' if source_leaves else 'original')
        if source_leaves:
            self.move('source', 'graveyard')
            self.move('other_copy', 'gone')
        else:
            self.move('original', 'hand')
        self.move('original_copy', 'gone')

    def parallax(self):
        hand = self.csvid == NEXUS
        zone = 'hand' if hand else 'battlefield'
        names = ('Forest', 'Island') if self.csvid == TIDE else ('Goblin Tinkerer', 'Prodigal Sorcerer')
        self.add('victim', names[0], self.foe, zone, tapped=not hand and self.csvid == TIDE)
        pair = self.mode == 'both_owners'
        if pair:
            self.add('second', names[1], self.actor, zone, tapped=self.csvid == TIDE)
        if hand and self.mode == 'foreign_hand':
            self.hand('unselected', 'Island', self.foe)
        self.add('unlinked', 'Mountain', self.foe, 'exile')
        if self.mode != 'fading':
            self.hand('exit', 'Disperse')
        self.cast('source', destination='battlefield')
        fade = 5
        self.counter('fade', fade)
        for ref in ('victim', 'second') if pair else ('victim',):
            self.activate_pending(ZERO, target=None if hand else ref)
            fade -= 1
            self.counter('fade', fade)
            if hand:
                self.named_zone(ref)
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            self.move(ref, 'exile')
            self.zone_id(ref, 'exile', 1)
        if self.mode == 'fading':
            for remaining in range(fade-1, -1, -1):
                self.owner_upkeep()
                self.counter('fade', remaining)
            self.owner_upkeep()
            self.move('source', 'graveyard')
        else:
            if hand:
                self.emit('advance_turn')
                self.emit('can_activate', 'source', value=0)
            self.cast('exit', target='source')
            self.move('source', 'hand')
        self.move('victim', zone)
        if pair:
            self.move('second', zone)
        self.zone_id('unlinked', 'exile', 1)

    def acid(self):
        owner = self.actor if self.mode == 'own_target' else self.foe
        self.field('host', "Traveler's Amulet", owner)
        self.field('other', 'Goblin Tinkerer', owner)
        vanish = self.mode == 'vanish'
        if not vanish:
            self.hand('exit', 'Disperse')
        self.cast('source', target='host', destination='battlefield')
        self.counter('time', 3)
        self.prop('host', 'in_play', 1)
        if vanish:
            for remaining in (2, 1):
                self.owner_upkeep()
                self.counter('time', remaining)
                self.prop('host', 'in_play', 1)
            self.owner_upkeep()
            self.move('source', 'graveyard')
        else:
            self.cast('exit', target='source')
            self.move('source', 'hand')
        self.move('host', 'graveyard')
        self.size('other')

    def nets(self):
        self.field('raider', 'Goblin Tinkerer', self.foe)
        if self.mode == 'release':
            self.hand('exit', 'Unsummon')
            self.add('unlinked', 'Prodigal Sorcerer', self.foe, 'exile')
        self.cast('source', destination='battlefield')
        self.size('source')
        if self.mode == 'defender':
            self.round_trip_turn()
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.prop('source', 'tapped', 0)
            self.emit('finish_attackers')
            return
        self.emit('advance_turn')
        self.emit('begin_attackers')
        self.emit('attack', 'raider', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.mode == 'release':
            self.emit('block', 'source', 'raider', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        if self.mode == 'release':
            self.prop('raider', 'in_play', 1)
            self.prop('source', 'damage', 1)
        else:
            self.life[self.actor] -= 1
        self.emit('end_combat')
        self.emit('resolve')
        if self.mode == 'release':
            self.move('raider', 'exile')
            self.receipt()
            self.cast('exit', target='source')
            self.move('source', 'hand')
            self.move('raider', 'battlefield')

    def artisan(self):
        owner = self.foe if self.mode == 'delayed_foreign' else self.actor
        is_creature = self.mode != 'noncreature'
        self.add('drawn', 'Goblin Tinkerer' if is_creature else 'Forest', owner, 'library')
        if is_creature:
            self.hand('exit', 'Unsummon')
        self.cast('source', destination='battlefield')
        self.prop('source', 'sick', 1)
        self.round_trip_turn()
        self.activate_pending((2, 0, 1, 0, 0, 0), target_player=owner, tap=True)
        self.named_zone('drawn')
        self.emit('resolve')
        self.emit('assert_choices_consumed')
        self.move('drawn', 'exile')
        if is_creature:
            self.token('copy', 'Goblin Tinkerer', owner)
            self.emit('assert_matching_permanents', player=owner, index=self.rows['Goblin Tinkerer']['csvid'],
                      prop='token', value=1)
            self.receipt()
            self.cast('exit', target='source')
            self.move('source', 'hand')
            self.zone_id('copy', 'battlefield', 1)
            self.emit('advance_turn')
            self.move('copy', 'gone')

    def egg(self):
        creature = self.mode != 'noncreature'
        self.hand('imprint', 'Goblin Tinkerer' if creature else 'Forest')
        self.hand('exit', 'Murder')
        self.pending('source')
        self.emit('observe_search_filters', value=1)
        decline = self.mode == 'decline'
        if decline:
            self.emit('choose_zone', player=self.actor, value=-1)
        else:
            self.named_zone('imprint')
        self.settle('source', 'battlefield')
        self.emit('assert_offered_legal_card', value=1, index=self.rows[self.cards['imprint']['name']]['csvid'])
        if not decline:
            self.move('imprint', 'exile')
        self.receipt()
        self.cast('exit', target='source')
        self.move('source', 'graveyard')
        if creature and not decline:
            self.move('imprint', 'battlefield')

    def vault(self):
        empty = self.mode == 'empty'
        if not empty:
            self.add('top', 'Forest', self.actor, 'library')
            self.add('next', 'Island', self.actor, 'library')
        returning = self.mode == 'return'
        if returning:
            self.hand('discard', 'Mountain')
        else:
            self.hand('exit', 'Disperse')
        self.cast('source', destination='battlefield')
        for top in ('top', 'next') if returning else ('top',):
            if top == 'next':
                self.owner_upkeep()
            self.activate_pending((2, 0, 0, 0, 0, 0), mode=0, tap=True)
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            if not empty:
                self.move(top, 'exile')
            self.receipt()
        if returning:
            self.activate_pending(ZERO, mode=1)
            self.move('source', 'graveyard')
            self.zone_id('source', 'graveyard', 1)
            self.zone_id('discard', 'hand', 1)
            if self.actor == 0:
                self.named_zone('discard')
            self.emit('resolve')
            self.emit('assert_choices_consumed')
            self.move('discard', 'graveyard')
            self.move('top', 'hand')
            self.move('next', 'hand')
        else:
            self.cast('exit', target='source')
            self.move('source', 'hand')
            if not empty:
                self.move('top', 'graveyard')

    def damage_spell(self):
        self.emit('advance_turn')
        self.pend_spell('damage', target_player=self.actor)

    def game_loss(self):
        self.emit('assert_stack', value=0)
        self.steps.append(step('advance_scheduled_turn', value=-1))
        self.game_terminal = True

    def lich(self):
        replacement = self.csvid == NEFARIOUS
        if self.mode == 'draw':
            self.hand('gain', 'Renewed Faith')
            for i, name in enumerate(('Forest', 'Forest', 'Forest', 'Island', 'Island', 'Island', 'Mountain')):
                self.add(f'library_{i}', name, self.actor, 'library')
        elif self.mode in ('graveyard', 'leaves'):
            self.hand('exit', 'Disperse' if replacement else 'Disenchant')
        elif self.mode == 'damage':
            if self.actor == 0:
                self.field('offering', "Traveler's Amulet")
            self.hand('damage', 'Lava Dart', self.foe)
        else:
            if self.mode == 'replace_damage':
                for i, name in enumerate(('Forest', 'Island', 'Mountain')):
                    self.add(f'fodder_{i}', name, self.actor, 'graveyard')
            self.hand('damage', 'Shock', self.foe)
        self.cast('source', destination='battlefield')
        if not replacement:
            self.life[self.actor] = 0
        self.receipt()
        if self.mode == 'draw':
            self.cast('gain')
            for i in range(6):
                self.move(f'library_{i}', 'hand')
            self.emit('assert_zone_card', player=self.actor, zone=ZONES['library'], value=self.rows['Mountain']['csvid'])
        elif self.mode == 'graveyard':
            self.pend_spell('exit', target='source')
            self.steps.append(step('resolve_duel_result', value=self.actor))
            self.game_terminal = True
        elif self.mode == 'leaves':
            self.cast('exit', target='source')
            self.move('source', 'hand')
            self.zone_id('source', 'hand', 1)
            self.game_loss()
        elif self.mode == 'damage':
            self.damage_spell()
            if self.actor == 0:
                self.emit('choose_target', 'offering', player=self.actor)
                self.settle('damage', input_receipt='assert_unused_choices')
            else:
                self.steps.append(step('resolve_duel_result', value=self.actor))
                self.game_terminal = True
                return
            self.move('offering', 'graveyard')
            self.life[self.actor] -= 1
        else:
            has_cards = self.mode == 'replace_damage'
            self.damage_spell()
            if has_cards:
                for i in (0, 1):
                    self.named_zone(f'fodder_{i}')
                    self.move(f'fodder_{i}', 'exile')
            self.settle('damage')
            if not has_cards:
                self.game_loss()

    def algae(self):
        self.field('land', 'Forest' if self.mode == 'non_swamp' else 'Swamp', self.foe)
        if self.mode == 'non_swamp':
            self.fund(self.actor, cost_from_oracle(self.oracle[self.cards['source']['name']]))
            self.emit('can_cast', 'source', value=0)
            return
        self.cast('source', target='land', destination='battlefield')
        if self.mode == 'exile':
            self.cast_exit_spell('Revoke Existence', destination='exile')
            self.prop('land', 'in_play', 1)
            return
        self.emit('activate', 'land', value=1)
        pool = [0] * 6
        pool[COLORS['B']] = 1
        self.wallet[self.foe] = tuple(pool)
        self.emit('resolve')
        self.move('land', 'graveyard')
        self.move('source', 'hand')

    def war_beast(self):
        lose_land = self.mode != 'no_land'
        if lose_land:
            self.field('land', 'Forest', tapped=True)
        self.field('other', 'Forest', self.foe)
        self.cast('source', destination='battlefield')
        self.size('source')
        bounce = self.mode != 'destroy'
        self.hand('exit', 'Unsummon' if bounce else 'Murder')
        self.pend_spell('exit', target='source')
        if lose_land and self.actor == 0:
            self.emit('choose_target', 'land', player=self.actor)
        self.settle('exit')
        self.move('source', 'hand' if bounce else 'graveyard')
        if lose_land:
            self.move('land', 'graveyard')
        self.life[self.actor] -= 1

    def nevermaker(self):
        mode = self.mode
        if mode != 'no_target':
            self.field('victim', 'Goblin Tinkerer', self.foe)
        self.field('land', 'Forest', self.foe)
        if mode in ('bounce', 'no_target'):
            self.hand('exit', 'Unsummon')
        self.pend_spell('source', mode=int(mode == 'evoke'))
        if mode == 'evoke':
            self.emit('choose_target', 'victim', player=self.actor)
        self.settle('source', 'graveyard' if mode == 'evoke' else 'battlefield',
                    input_receipt='assert_unused_choices' if mode == 'evoke' else 'assert_choices_consumed')
        if mode == 'flying':
            self.size('source')
            self.round_trip_turn()
            self.battle('source', attempts=(('victim', False),))
            return
        if mode != 'evoke':
            self.pend_spell('exit', target='source')
            if mode != 'no_target':
                self.emit('choose_target', 'victim', player=self.actor)
            self.settle('exit', input_receipt='assert_unused_choices' if mode == 'bounce'
                        else 'assert_choices_consumed')
            self.move('source', 'hand')
        if mode != 'no_target':
            self.move('victim', 'library')
            self.emit('assert_zone_card', player=self.foe, zone=ZONES['library'],
                      value=self.rows['Goblin Tinkerer']['csvid'])

    def play(self):
        self.game_terminal = False
        self.begin_main()
        self.action()
        if not self.game_terminal:
            self.receipt()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    source = profiles()['sources'].get(record.get('name'))
    if (source is None or not exact(source['oracle'], record)
            or source['registry']['csvid'] != csvid or csvid not in MODES):
        raise ValueError('Exact implemented departure record required')
    return {'csvid': csvid}


def variants(model):
    return [f'{mode}_{actor}' for mode in MODES[model['csvid']] for actor in (0, 1)]


def validate_witnesses(oracle, rows):
    pinned = profiles()
    if (set(pinned) != {'sources', 'witnesses'}
            or {source['registry']['csvid'] for source in pinned['sources'].values()} != set(MODES)):
        raise ValueError('Exact departure profile scope required')
    for group in pinned.values():
        for name, source in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            if (not exact(source, dict(oracle=record, registry=row)) or record.get('layout') != 'normal'
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented departure source or witness: {name}')


def spec(declaration, record):
    pinned = profiles()['sources'].get((record or {}).get('name'))
    if not pinned:
        raise ValueError('Whole pinned departure primary required')
    model = classify(record, pinned['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(chosen, list) or not chosen
            or any(type(v) is not str for v in chosen) or len(chosen) != len(set(chosen))
            or set(chosen) - set(variants(model))):
        raise ValueError('Exact scope and distinct supported departure variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact primary and bounded variants required')
    factory = Board if csvid in (AURIFICATION, THIEF, SHELL, TOMB) else LinkedBoard
    gaps = {
        SHELL: [
            'Public face-down visibility is unobserved. The test uses exact linked card identity and the actual mandatory reveal input.',
            'The test observes bottom order and retains the fifth card. Initial chooser bounds cannot be read before bottom-order selectors overwrite them.',
        ],
        TOMB: ['Black production and restoration observe Mire effects. The engine’s numeric Mire counter has no exposed observer.'],
        EGG: ['The test observes the imprinted identity and creature-only return. Public face-down visibility has no exposed observer.'],
        VAULT: ['The test observes linked exile identity. Public face-down visibility has no exposed observer.'],
    }
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        board = factory(oracle[name], csvid, mode, int(seat), rows, oracle)
        board.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps.get(csvid, []))
        cases.append(dict(name=label(csvid, mode, int(seat)), card=name, fixture='empty_main', seed=SEED,
                          setup=board.setup, steps=board.steps, _recipe=metadata))
    return cases
