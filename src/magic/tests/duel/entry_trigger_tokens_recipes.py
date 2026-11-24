"""Entry-token recipes with exact whole-Oracle sources and real board interactions."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'entry_trigger_tokens'
PROFILE = Path(__file__).with_name('data') / 'entry_trigger_tokens_wave8_profiles.json'

CORAL, ELITE, MASTERY, BOND = 14087, 15115, 16960, 16240
PREFIX, SEED = 'EntryTriggerTokensRecipe', 3152
MODES = {
    CORAL: ('islandwalk', 'own_island', 'nonisland', 'squid_type', 'defend'),
    ELITE: ('alone', 'owner_elf', 'foe_elf', 'tapped_elf', 'changeling', 'elf_removed', 'nonelf'),
    MASTERY: ('typed', 'remove_mastery', 'repeated', 'wrong_color'),
    BOND: ('own_host', 'foreign_host', 'remove_aura', 'target_gone', 'no_legal'),
}
HAND, BATTLE, LIBRARY, GRAVE = range(4)
ZERO = (0,) * 6
SQUID, ELF_TOKEN, ZOMBIE, WOLF = 'Squid', 'Elf Warrior', 'Zombie', 'Wolf'
BEAR, ISLAND, FOREST = 'Grizzly Bears', 'Island', 'Forest'
COAT, SKULKER = 'Coat of Arms', 'Chasm Skulker'
ELF, CHANGELING, CORPSE = 'Elvish Mystic', 'Amoeboid Changeling', 'Walking Corpse'
BOUNCE, MURDER, DISENCHANT, RELIC = 'Boomerang', 'Murder', 'Disenchant', 'Darksteel Relic'
WITNESSES = {SQUID, ELF_TOKEN, ZOMBIE, WOLF, BEAR, ISLAND, FOREST, COAT, SKULKER, ELF, CHANGELING,
             CORPSE, BOUNCE, MURDER, DISENCHANT, RELIC}
TOKENS = {SQUID: (18832, 4, 1, 1, ()), ELF_TOKEN: (18732, 8, 1, 1, ('Elf','Warrior')),
          ZOMBIE: (18860, 2, 2, 2, ('Zombie',)), WOLF: (18856, 8, 2, 2, ('Wolf',))}

class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.mode, self.owner, self.foe = mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        self.active = owner
        self.add('source', self.name, owner, HAND)
        self.zone_only = set()
        self.setup_witnesses()

    def setup_witnesses(self):
        policies = {CORAL: self.coral_setup, ELITE: self.elite_setup, MASTERY: self.mastery_setup, BOND: self.bond_setup}
        policies[self.id]()

    def add(self, ref, name, player, zone=BATTLE, *, tapped=False):
        assert ref not in self.cards
        placed = dict(name=name, ref=ref, player=player, zone=ZONES[zone])
        if tapped:
            placed['state']=['tapped']
        self.setup.append(placed)
        self.cards[ref] = [name, player, zone]

    def emit(self, op, ref='', other='', *, prop='', player=0, value=0, zone=HAND, index=0, color=0):
        action = dict(op=op)
        if ref:
            action['ref'] = ref
        if other:
            action['other'] = other
        if prop:
            action['counter_kind' if op == 'assert_counter' else 'property'] = prop
        if op in SEATED:
            action['player'] = player
        if op in VALUES:
            action['value'] = value
        if op in ZONE_OPS:
            action['zone'] = ZONES[zone]
        if op in INDEXED:
            action['index'] = index
        if op in ('set_mana', 'assert_mana'):
            action['color'] = color
        self.steps.append(action)

    def see(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, mana):
        self.pools[player] = tuple(mana)
        for color, count in enumerate(mana):
            self.emit('set_mana', player=player, color=color, value=count)
        self.emit('refresh')

    def see_mana(self, player):
        for color, count in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, color=color, value=count)

    def move(self, ref, zone):
        self.cards[ref][2] = zone

    def state(self):
        self.emit('assert_stack')
        for p in (0, 1):
            self.see_mana(p)
            self.emit('assert_life', player=p, value=self.life[p])
            for zone in self.observed_zones():
                items = Counter(name for name, player, where in self.cards.values()
                                if where == zone and player == p)
                self.emit('assert_zone', player=p, zone=zone, value=items.total())
                if zone != BATTLE:
                    for name, count in sorted(items.items()):
                        self.emit('assert_zone_multiplicity', player=p, zone=zone, value=count,
                                  index=self.rows[name]['csvid'])
        for ref, (_, _, zone) in self.cards.items():
            if ref not in self.zone_only:
                self.see(ref, 'in_play', int(zone == BATTLE))

    def observed_zones(self):
        return HAND, BATTLE, GRAVE

    def settle(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack')

    def put_on_stack(self, ref, targets=(), *, response=False):
        name, player, zone = self.cards[ref]
        assert zone == HAND
        self.fund(player, cost_from_oracle(self.oracle[name]))
        for target in targets:
            self.emit('choose_target', target, player=player)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=2 if response else 1)
        self.pools[player] = ZERO
        self.see_mana(player)

    def cast(self, ref, targets=()):
        self.put_on_stack(ref, targets)
        self.settle()
        self.move(ref, GRAVE if self.oracle[self.cards[ref][0]]['type_line'] == 'Instant' else BATTLE)

    def response(self, ref, target, effect):
        self.emit('announce_spell', 'source')
        self.put_on_stack(ref, (target,), response=True)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.move(ref, GRAVE)
        self.move(target, effect)

    def visit(self, player, *, scheduled=False):
        assert player == 1-self.active and self.pools == [ZERO, ZERO]
        if scheduled:
            self.emit('advance_scheduled_turn', value=player)
        else:
            self.emit('advance_turn')
        self.emit('untap_step', player=player)
        self.emit('turn', player=player)
        self.active = player

    def attack(self, refs, *, denied=(), vigilant=()):
        self.emit('begin_attackers')
        for ref in refs:
            self.emit('attack', ref, value=1)
            self.see(ref, 'attacking', 1)
            self.see(ref, 'tapped', int(ref not in vigilant))
        for ref in denied:
            self.emit('attack', ref, value=0)
        self.emit('finish_attackers')
        self.settle()

    def blockers(self, blocks=()):
        self.emit('begin_blockers')
        for blocker, attacker, legal in blocks:
            self.emit('block', blocker, attacker, value=legal)
        self.emit('advance_combat')
        self.settle()
        committed = {attacker for _, attacker, legal in blocks if legal}
        for _, attacker, _ in blocks:
            self.see(attacker, 'blocked', int(attacker in committed))

    def damage(self, *, hit=0, dead=(), marked=()):
        self.emit('combat_damage')
        self.settle()
        for ref in dead:
            self.move(ref, GRAVE)
        for ref, damage in marked:
            self.see(ref, 'damage', damage)
        self.life[1-self.active] -= hit
        self.emit('end_combat')
        self.state()

    def stats(self, ref, bonus=0):
        name = self.cards[ref][0]
        for prop in ('power', 'toughness'):
            self.see(ref, prop, int(self.oracle[name][prop])+bonus)

    def coral_setup(self):
        if self.mode == 'squid_type':
            self.add('coat', COAT, self.owner)
            self.add('squid_witness', SKULKER, self.owner)
            return
        self.add('guard', BEAR, self.foe)
        if self.mode == 'islandwalk':
            self.add('land', ISLAND, self.foe)
        elif self.mode == 'own_island':
            self.add('land', ISLAND, self.owner)
        elif self.mode == 'nonisland':
            self.add('land', FOREST, self.foe)

    def elite_setup(self):
        if self.mode in ('owner_elf', 'tapped_elf', 'elf_removed', 'foe_elf'):
            self.add('mentor', ELF, self.foe if self.mode == 'foe_elf' else self.owner,
                     tapped=self.mode == 'tapped_elf')
        elif self.mode in ('changeling', 'nonelf'):
            self.add('mentor', CHANGELING if self.mode == 'changeling' else BEAR, self.owner)
        if self.mode == 'elf_removed':
            self.add('answer', BOUNCE, self.foe, HAND)

    def mastery_setup(self):
        if self.mode != 'wrong_color':
            self.add('own_zombie', CORPSE, self.owner)
            self.add('enemy_zombie', CORPSE, self.foe)
            self.add('other', BEAR, self.owner)
        if self.mode == 'repeated':
            self.add('second', self.name, self.owner, HAND)
        if self.mode in ('remove_mastery', 'repeated'):
            self.add('answer', DISENCHANT, self.foe, HAND)

    def bond_setup(self):
        if self.mode == 'no_legal':
            self.add('not_creature', RELIC, self.foe)
            return
        self.add('host', BEAR, self.foe if self.mode == 'foreign_host' else self.owner)
        self.add('other', BEAR, self.owner)
        if self.mode in ('remove_aura', 'target_gone'):
            self.add('answer', MURDER if self.mode == 'target_gone' else DISENCHANT, self.foe, HAND)

    def observe_tokens(self, name, start, count, *, bonus=0, seat=None, sick=True, prefix='token'):
        ident, color, power, toughness, types = TOKENS[name]
        seat = self.owner if seat is None else seat
        for index in range(start, start+count):
            ref = f'{prefix}_{index}'
            self.emit('bind_permanent_copy', ref, player=seat, value=ident, index=index)
            self.cards[ref] = [name, seat, BATTLE]
            self.see(ref, 'tapped', 0)
            if sick:
                self.see(ref, 'sick', 1)
        self.token_properties(name, bonus=bonus, seat=seat)

    def token_properties(self, name, *, bonus=0, seat=None):
        ident, color, power, toughness, types = TOKENS[name]
        fields = [('token', 1), ('color', color), ('artifact', 0),
                  ('power', power+bonus), ('toughness', toughness+bonus)]
        fields.extend((f'subtype_{subtype}', 1) for subtype in types)
        for prop, value in fields:
            self.emit('assert_matching_permanents', player=self.owner if seat is None else seat, prop=prop, value=value, index=ident)

    def coral(self):
        if self.mode == 'squid_type':
            self.stats('squid_witness')
        self.cast('source')
        self.stats('source')
        self.observe_tokens(SQUID, 0, 1, bonus=int(self.mode == 'squid_type'))
        self.state()
        if self.mode == 'squid_type':
            self.stats('squid_witness', 1)
            return
        self.visit(self.foe)
        if self.mode == 'defend':
            self.attack(('guard',))
            self.blockers((('source', 'guard', 1),))
            self.damage(marked=(('source',2),('guard',1)))
            return
        self.visit(self.owner)
        self.see('source', 'sick', 0)
        self.attack(('token_0',), denied=('source',))
        walks = self.mode == 'islandwalk'
        self.blockers((('guard', 'token_0', int(not walks)),))
        if walks:
            self.damage(hit=1)
        else:
            self.emit('combat_damage')
            self.settle()
            self.cards.pop('token_0')
            self.see('token_0', 'in_play', 0)
            self.see('guard', 'damage', 1)
            self.emit('end_combat')
            self.state()

    def elite(self):
        if self.mode == 'elf_removed':
            self.put_on_stack('source')
            self.response('answer', 'mentor', HAND)
            self.settle()
            self.move('source', BATTLE)
        else:
            self.cast('source')
        if self.mode in ('owner_elf', 'tapped_elf', 'changeling'):
            self.observe_tokens(ELF_TOKEN, 0, 1)
        self.stats('source')
        if self.mode == 'tapped_elf':
            self.see('mentor', 'tapped', 1)
        self.state()

    def mastery_stats(self, grow):
        self.stats('own_zombie', grow)
        self.stats('enemy_zombie')
        self.stats('other')

    def mastery(self):
        if self.mode == 'wrong_color':
            self.fund(self.owner, (5,0,0,0,0,0))
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            self.state()
            return
        self.mastery_stats(0)
        self.cast('source')
        self.observe_tokens(ZOMBIE, 0, 2, bonus=1)
        self.mastery_stats(1)
        self.state()
        if self.mode == 'repeated':
            self.cast('second')
            self.observe_tokens(ZOMBIE, 2, 2, bonus=2)
            self.mastery_stats(2)
            self.state()
        if self.mode in ('remove_mastery', 'repeated'):
            self.cast('answer', ('source',))
            self.move('source', GRAVE)
            remaining = int(self.mode == 'repeated')
            self.token_properties(ZOMBIE, bonus=remaining)
            self.mastery_stats(remaining)
            self.state()

    def bond(self):
        if self.mode == 'no_legal':
            self.fund(self.owner, cost_from_oracle(self.oracle[self.name]))
            self.emit('can_cast', 'source', value=0)
            self.emit('cast', 'source', value=0)
            self.emit('assert_unused_choices', value=0)
            self.state()
            return
        if self.mode == 'target_gone':
            self.put_on_stack('source', ('host',))
            self.response('answer', 'host', GRAVE)
            self.settle()
            self.move('source', GRAVE)
            self.stats('other')
            self.state()
            return
        self.cast('source', ('host',))
        self.observe_tokens(WOLF, 0, 1)
        self.stats('host', 2)
        self.stats('other')
        self.state()
        if self.mode == 'remove_aura':
            self.cast('answer', ('source',))
            self.move('source', GRAVE)
            self.stats('host')
            self.stats('other')
            self.token_properties(WOLF)
            self.state()

    def run(self):
        self.emit('turn', player=self.owner)
        self.fund(self.foe, ZERO)
        self.fund(self.owner, ZERO)
        policies = {CORAL: self.coral, ELITE: self.elite, MASTERY: self.mastery, BOND: self.bond}
        policies[self.id]()

GATES, EMISSARY, CARAVAN, FISTS, SERVANT, ORNITHARCH = 13229, 15801, 17038, 7842, 15241, 13775
ALPHA, BELFRY, SCION, PRIEST, AUTOCRAT, VALOR = 17498, 8024, 13217, 15198, 1647, 12762
KNIGHT, SPIRIT, WARRIOR, SAPROLING = 'Knight', 'Spirit', 'Warrior', 'Saproling'
BIRD, DINOSAUR, BAT, CENTAUR, DEMON, SERF = 'Bird', 'Dinosaur', 'Bat', 'Centaur', 'Demon', 'Serf'
AZORIUS, ORZHOV = 'Azorius Guildgate', 'Orzhov Guildgate'
CAD, GIANT, GROWTH, BOOK, RAPTOR = 'Eager Cadet', 'Hill Giant', 'Giant Growth', 'Spellbook', 'Raptor Companion'
CALL, BLINK = 'Call of the Conclave', 'Cloudshift'
EXILE = 4
TOKENS.update({
    KNIGHT: (18776,32,2,2,('Knight',)),
    SPIRIT: (18829,32,1,1,('Spirit',)),
    WARRIOR: (18852,32,1,1,('Warrior',)),
    SAPROLING: (18813,8,1,1,('Saproling',)),
    BIRD: (18696,32,1,1,('Bird',)),
    DINOSAUR: (18717,8,3,3,()),
    BAT: (18693,2,1,1,('Bat',)),
    CENTAUR: (18708,8,3,3,('Centaur',)),
    DEMON: (18714,2,5,5,()),
    SERF: (18815,2,0,1,()),
})
WITNESSES.update((*TOKENS, AZORIUS, ORZHOV, CAD, GIANT, GROWTH, BOOK, RAPTOR, CALL, BLINK))
FOLLOW_MODES = {
    GATES: ('two', 'one_and_foe', 'non_gates'),
    EMISSARY: ('enemy_death', 'own_death', 'past_turn', 'artifact_only'),
    CARAVAN: ('own_attacked', 'foe_tapped', 'only_land', 'untapped'),
    FISTS: ('trample', 'foreign_host', 'removed'),
    SERVANT: ('none', 'one', 'three', 'foreign'),
    ORNITHARCH: ('tribute_paid', 'tribute_declined'),
    ALPHA: ('trample', 'haste_team', 'lord_removed'),
    BELFRY: ('entry', 'haunt', 'unlinked'),
    SCION: ('populate_existing', 'blink', 'bird_only'),
    PRIEST: ('entry', 'upkeep', 'after_death'),
    AUTOCRAT: ('entry', 'global_leave', 'dies'),
    VALOR: ('vigilance', 'foreign_host', 'removed'),
}
ALL_MODES = MODES | FOLLOW_MODES
ZONES = ('hand','battlefield','library','graveyard','exile')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity','assert_matching_permanents','bind_permanent_copy','bind_permanent'}
SEATED = {'turn','untap_step','upkeep_step','choose_target','choose_dialog','set_mana','assert_mana',
          'assert_life','bind_permanent_copy','bind_permanent','assert_matching_permanents'} | ZONE_OPS
VALUES = {'advance_scheduled_turn','cast','cast_response','can_cast','activate','choose_dialog','present_ai_dialogs','set_mana','assert_mana',
          'assert_life','assert_stack','assert_card','assert_unused_choices','assert_matching_permanents',
          'bind_permanent_copy','bind_permanent','attack','block','assert_counter'} | ZONE_OPS
GAPS = {
    CORAL: ['Squid has no direct subtype observer. Actual Coat of Arms and a distinct Squid provide the subtype witness.'],
    ALPHA: ['The direct token observer lacks a Dinosaur subtype enum. The exact token identity and actual team haste distinguish the typed effect.'],
    SCION: ['The mandatory Human populate prompt cannot name the Bird spawned mid-resolution through the current named-reference queue. Human cases use a genuine preexisting Centaur; the AI-only new-Bird copy is exercised separately.'],
    PRIEST: ['The direct subtype observer lacks Demon. The exact pinned token identity, body/color and actual flying are observed.'],
    AUTOCRAT: ['The direct subtype observer lacks Serf. Independent Serf identity and real global exile contrast with retained non-Serf tokens and creatures.'],
}


class FollowBoard(Board):
    def setup_witnesses(self):
        if self.id == GATES:
            if self.mode == 'non_gates':
                self.add('land_a', FOREST, self.owner)
                self.add('land_b', ISLAND, self.owner)
            else:
                self.add('land_a', AZORIUS, self.owner)
                self.add('land_b', ORZHOV, self.owner if self.mode == 'two' else self.foe, tapped=True)
        elif self.id == EMISSARY:
            self.add('victim', BOOK if self.mode == 'artifact_only' else BEAR,
                     self.owner if self.mode=='own_death' else self.foe)
            self.add('answer', DISENCHANT if self.mode == 'artifact_only' else MURDER, self.owner, HAND)
            self.add('guard', BEAR, self.foe)
        elif self.id == CARAVAN:
            self.add('tapper', BEAR, self.owner)
            if self.mode == 'foe_tapped':
                self.add('foe_tap', BEAR, self.foe, tapped=True)
            elif self.mode == 'only_land':
                self.add('land', FOREST, self.owner)
                self.add('spend', GROWTH, self.owner, HAND)
        elif self.id in (FISTS, VALOR):
            self.add('host', GIANT, self.foe if self.mode == 'foreign_host' else self.owner)
            self.add('decoy', BEAR, self.owner)
            if self.id == FISTS:
                self.add('guard', CAD, self.owner if self.mode == 'foreign_host' else self.foe)
            if self.mode == 'removed':
                self.add('answer', DISENCHANT, self.foe, HAND)
        elif self.id == SERVANT:
            copies = {'none':0,'one':1,'three':3,'foreign':2}[self.mode]
            for index in range(copies):
                ref = f'grave_servant_{index}'
                self.add(ref, self.name, self.foe if self.mode == 'foreign' else self.owner, GRAVE)
                self.zone_only.add(ref)
            self.add('different_zombie', CORPSE, self.owner, GRAVE)
            self.zone_only.add('different_zombie')
        elif self.id == ORNITHARCH:
            self.add('guard', BEAR, self.foe)
        elif self.id == ALPHA:
            if self.mode == 'trample':
                self.add('guard', CAD, self.foe)
            if self.mode == 'haste_team':
                self.add('dino', RAPTOR, self.owner, HAND)
                self.add('other', BEAR, self.owner, HAND)
            elif self.mode == 'lord_removed':
                self.add('answer', MURDER, self.foe, HAND)
        elif self.id == BELFRY:
            if self.mode == 'entry':
                self.add('guard', BEAR, self.foe)
            else:
                self.add('haunted', BEAR, self.foe)
                self.add('unlinked', BEAR, self.foe)
                self.add('answer', MURDER, self.foe, HAND)
                self.add('later', MURDER, self.foe, HAND)
        elif self.id == SCION:
            if self.mode != 'bird_only':
                self.add('call', CALL, self.owner, HAND)
            if self.mode == 'blink':
                self.add('blink', BLINK, self.owner, HAND)
            self.add('guard', BEAR, self.foe)
        elif self.id == PRIEST:
            if self.mode == 'entry':
                self.add('guard', BEAR, self.foe)
            if self.mode == 'after_death':
                self.add('answer', MURDER, self.foe, HAND)
        elif self.id == AUTOCRAT:
            if self.mode == 'global_leave':
                self.add('foreign_source', self.name, self.foe, HAND)
                self.add('call', CALL, self.owner, HAND)
                self.add('answer', BOUNCE, self.foe, HAND)
            elif self.mode == 'dies':
                self.add('answer', MURDER, self.foe, HAND)

    def observed_zones(self):
        return (HAND, BATTLE, GRAVE, EXILE) if self.id == BELFRY else (HAND, BATTLE, GRAVE)

    def cast(self, ref, targets=(), *, during=(), expected_unused=0):
        self.put_on_stack(ref,targets)
        for op, player, choice in during:
            if op == 'choose_target':
                self.emit(op, choice, player=player)
            else:
                self.emit(op, player=player, value=choice)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices',value=expected_unused)
        self.emit('assert_stack')
        name, _, _ = self.cards[ref]
        self.move(ref, GRAVE if self.oracle[name]['type_line'] in ('Instant','Sorcery') else BATTLE)

    def action(self, ref='source'):
        self.cast(ref)
        self.stats(ref)

    def cycle(self):
        self.visit(self.foe)
        self.visit(self.owner)

    def uphold(self, actor):
        assert actor == 1-self.active and self.pools == [ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=actor)
        self.emit('upkeep_step',player=actor)
        self.settle()
        self.emit('turn',player=actor)
        self.active = actor

    def attack_tokens(self, refs, *, damage, vigilant=(), deny=None):
        self.cycle()
        self.attack(refs,vigilant=vigilant)
        self.blockers((('guard',deny,0),) if deny else ())
        self.damage(hit=damage)

    def gatekeeper(self):
        self.action()
        yes = self.mode == 'two'
        if yes:
            self.observe_tokens(KNIGHT,0,1)
        self.state()
        if yes:
            self.attack_tokens(('token_0',),damage=2,vigilant=('token_0',))

    def emissary(self):
        self.cast('answer',('victim',))
        self.move('victim',GRAVE)
        if self.mode == 'past_turn':
            self.visit(self.foe, scheduled=True)
            self.visit(self.owner, scheduled=True)
        self.action()
        yes = self.mode in ('enemy_death','own_death')
        if yes:
            self.observe_tokens(SPIRIT,0,1)
        self.state()
        if yes:
            self.cycle()
            self.attack(('source','token_0'))
            self.blockers((('guard','source',0),('guard','token_0',0)))
            self.damage(hit=3)

    def caravan(self):
        if self.mode == 'own_attacked':
            self.attack(('tapper',))
            self.blockers()
            self.damage(hit=2)
            self.see('tapper','tapped',1)
        elif self.mode == 'only_land':
            self.emit('activate','land',value=1)
            self.emit('assert_stack')
            self.pools[self.owner]=(0,0,0,1,0,0)
            self.see_mana(self.owner)
            self.see('land','tapped',1)
            self.cast('spend',('tapper',))
            self.see('tapper','tapped',0)
        self.action()
        yes = self.mode == 'own_attacked'
        if yes:
            self.observe_tokens(WARRIOR,0,1)
        self.state()
        if yes:
            self.attack_tokens(('token_0',),damage=1,vigilant=('token_0',))

    def auras(self):
        self.cast('source',('host',))
        is_valor = self.id == VALOR
        self.observe_tokens(KNIGHT if is_valor else SAPROLING,0,1 if is_valor else 2)
        self.stats('host',2 if is_valor else 0)
        self.stats('decoy')
        self.state()
        if self.mode == 'removed':
            self.cast('answer',('source',))
            self.move('source',GRAVE)
            self.stats('host')
            self.state()
        if self.mode == 'foreign_host':
            self.visit(self.foe)
            self.attack(('host',),vigilant=('host',) if is_valor else ())
            if is_valor:
                self.blockers()
                self.damage(hit=5)
            else:
                self.blockers((('guard','host',1),))
                self.damage(hit=2,dead=('guard',),marked=(('host',1),))
            return
        if is_valor:
            self.cycle()
            self.attack(('host','token_0'),vigilant=('token_0',) if self.mode == 'removed' else ('host','token_0'))
            self.blockers()
            self.damage(hit=5 if self.mode == 'removed' else 7)
        else:
            self.attack(('host',))
            self.blockers((('guard','host',1),))
            self.damage(hit=0 if self.mode == 'removed' else 2,dead=('guard',),marked=(('host',1),))

    def servant(self):
        self.action()
        count = {'none':0,'one':1,'three':3,'foreign':0}[self.mode]
        if count:
            self.observe_tokens(ZOMBIE,0,count)
        self.state()

    def ornitharch(self):
        yes = self.mode == 'tribute_paid'
        self.emit('present_ai_dialogs',value=1)
        self.cast('source', during=(('choose_dialog',self.foe,int(not yes)),))
        self.emit('assert_counter','source',prop='plus1',value=2 if yes else 0)
        self.stats('source',2 if yes else 0)
        if not yes:
            self.observe_tokens(BIRD,0,2)
        self.state()
        self.cycle()
        refs=('source',) if yes else ('source','token_0','token_1')
        self.attack(refs)
        self.blockers(tuple(('guard',ref,0) for ref in refs))
        self.damage(hit=5)

    def regisaur(self):
        self.action()
        self.observe_tokens(DINOSAUR,0,1,sick=False)
        self.state()
        if self.mode == 'trample':
            self.attack(('token_0',),denied=('source',))
            self.blockers((('guard','token_0',1),))
            self.damage(hit=2,dead=('guard',),marked=(('token_0',1),))
        elif self.mode == 'haste_team':
            self.cast('dino')
            self.cast('other')
            self.attack(('dino','token_0'),denied=('source','other'))
            self.blockers()
            self.damage(hit=6)
        else:
            self.cast('answer',('source',))
            self.move('source',GRAVE)
            self.attack((),denied=('token_0',))
            self.blockers()
            self.damage()

    def belfry(self):
        self.action()
        self.observe_tokens(BAT,0,2)
        self.state()
        if self.mode == 'entry':
            self.cycle()
            refs = ('source','token_0','token_1')
            self.attack(refs)
            self.blockers(tuple(('guard',ref,0) for ref in refs))
            self.damage(hit=3)
            return
        self.cast('answer',('source',),during=(('choose_target',self.owner,'haunted'),))
        self.move('source',EXILE)
        self.see('source','in_play',0)
        self.zone_only.add('source')
        self.state()
        victim = 'haunted' if self.mode == 'haunt' else 'unlinked'
        self.cast('later',(victim,))
        self.move(victim,GRAVE)
        if self.mode == 'haunt':
            self.observe_tokens(BAT,2,2)
        self.state()

    def scion(self):
        if self.mode != 'bird_only':
            self.cast('call')
            self.observe_tokens(CENTAUR,0,1,prefix='centaur')
        choices = (('choose_target',self.owner,'centaur_0'),) if self.owner==0 else ()
        self.cast('source',during=choices)
        self.stats('source')
        if self.mode == 'bird_only':
            self.observe_tokens(BIRD,0,2)
        else:
            self.observe_tokens(BIRD,0,1)
            self.observe_tokens(CENTAUR,1,1,prefix='centaur')
        self.state()
        if self.mode == 'blink':
            # Legal fallback if the engine incorrectly fires the hand-cast-only
            # effect. Correct blink never presents populate and leaves it unused.
            self.cast('blink',('source',),during=choices,expected_unused=len(choices))
            self.cards.pop('source')
            self.emit('bind_permanent','returned',player=self.owner,value=self.id,index=-1)
            self.cards['returned']=[self.name,self.owner,BATTLE]
            self.stats('returned')
            self.state()
        if self.mode == 'populate_existing':
            self.attack_tokens(('token_0',),damage=1,deny='token_0')

    def priest(self):
        self.action()
        self.observe_tokens(DEMON,0,1)
        self.state()
        if self.mode == 'entry':
            self.attack_tokens(('token_0',),damage=5,deny='token_0')
        else:
            if self.mode == 'after_death':
                self.cast('answer',('source',))
                self.move('source',GRAVE)
            self.uphold(self.foe)
            self.state()
            self.uphold(self.owner)
            if self.mode == 'upkeep':
                self.life[self.owner]-=2
            self.state()

    def autocrat(self):
        self.action()
        self.observe_tokens(SERF,0,3)
        self.state()
        if self.mode=='entry':
            return
        if self.mode=='global_leave':
            self.cast('call')
            self.observe_tokens(CENTAUR,0,1,prefix='centaur')
            self.visit(self.foe)
            self.cast('foreign_source')
            self.observe_tokens(SERF,0,3,seat=self.foe,prefix='foreign')
            self.state()
        self.cast('answer',('source',))
        self.move('source',HAND if self.mode=='global_leave' else GRAVE)
        prefixes = ('token','foreign') if self.mode=='global_leave' else ('token',)
        for prefix in prefixes:
            for i in range(3):
                ref=f'{prefix}_{i}'
                self.cards.pop(ref)
                self.see(ref,'in_play',0)
        self.state()

    def run(self):
        self.emit('turn',player=self.owner)
        self.fund(self.foe,ZERO)
        self.fund(self.owner,ZERO)
        automation={GATES:self.gatekeeper,EMISSARY:self.emissary,CARAVAN:self.caravan,FISTS:self.auras,
                    SERVANT:self.servant,ORNITHARCH:self.ornitharch,ALPHA:self.regisaur,BELFRY:self.belfry,
                    SCION:self.scion,PRIEST:self.priest,AUTOCRAT:self.autocrat,VALOR:self.auras}
        automation[self.id]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record,pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid'] != csvid or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal entry token primary required')
    return dict(csvid=csvid, mana=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in ALL_MODES[shape['csvid']]
            for seat in ((1,) if mode == 'bird_only' else (0,1))]


def validate_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported entry token variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned entry token primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared entry token parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins = profiles()
    if (set(pins) != {'sources','witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != ALL_MODES.keys()):
        raise ValueError('complete entry token source and witness pins required')
    for group,entries in pins.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('entry token source or witness changed: '+name)
            if group == 'sources':
                if row.get('kind') != 'card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented entry token primary required')
                classify(record,row['csvid'])
            elif name in TOKENS and (row['csvid'] != TOKENS[name][0] or record['layout'] != 'token'):
                raise ValueError('exact physical token template required')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid'] != csvid or shape['csvid'] != csvid:
        raise ValueError('exact registered entry token source required')
    validate_variants(shape,selected)
    validate_witnesses(oracle,rows)
    result = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board_type = FollowBoard if csvid in FOLLOW_MODES else Board
        game = board_type(rows[name],mode,int(seat),rows,oracle)
        game.run()
        result.append(dict(name=f'{PREFIX}_{csvid}_{variant}',card=name,fixture='empty_main',seed=SEED,
                           setup=game.setup,steps=game.steps,
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                        oracle_text=declaration['oracle_text'],untested_components=GAPS.get(csvid,[]))))
    return result
