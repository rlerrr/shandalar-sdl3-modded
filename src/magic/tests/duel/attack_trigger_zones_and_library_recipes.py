"""Paid attack sources with exact hand, library, exile, and return events."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'attack_trigger_zones_and_library'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'AttackTriggerZonesAndLibraryRecipe'
SEED = 3012
ROC, SKAAB, GUIDE, MAGE = 12823, 12393, 10547, 9147
WHALE, SCUTTLER, SENTINEL, THRULL, THIEF, BANE, SAGE, SPHINX, DRAGON, BOMAT = (
    13274, 16789, 13804, 13086, 16128, 15267, 14615, 11108, 10351, 16300,
)
MODES = {
    ROC: ('boundary', 'decline', 'no_match', 'owner_return', 'flight'),
    SKAAB: ('own', 'shroud', 'borrowed', 'no_other', 'other_attacks'),
    GUIDE: ('land', 'nonland', 'empty', 'repeat', 'other_attacks'),
    MAGE: ('own_etb', 'foe', 'borrowed', 'source_dies', 'no_other', 'flight'),
    WHALE: ('exile_stays', 'leaves', 'owner_return', 'decline', 'islandwalk', 'no_island'),
    SCUTTLER: ('artifact', 'borrowed', 'no_artifact'),
    SENTINEL: ('enchantment', 'decline', 'shroud', 'wrong_owner', 'flight'),
    THRULL: ('artifact', 'creature', 'enchantment', 'decline', 'shroud', 'extort_black', 'extort_white', 'extort_short'),
    THIEF: ('copy', 'copy_flying', 'decline', 'token_only', 'no_other'),
    BANE: ('two', 'only_one', 'empty', 'planeswalker'),
    SAGE: ('smaller', 'equal', 'own', 'prowess', 'decline'),
    SPHINX: ('match', 'miss', 'flight', 'other_attacks', 'short_fee'),
    DRAGON: ('large_discard', 'discard_drawn', 'zero_discard', 'flight'),
    BOMAT: ('recover', 'repeat', 'short', 'died', 'two_couriers'),
}
ORIGINAL = (ROC, SKAAB, GUIDE, MAGE)
OPTIONAL_TRIGGER = (ROC, WHALE, THRULL, THIEF, SAGE)
ZERO = (0,) * 6
ZONE = tuple(('hand', 'battlefield', 'library', 'graveyard', 'exile'))


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal attack zone and library rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned attack zone and library source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned attack zone and library declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported attack zone and library variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') not in ('card', 'token') or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and (row['kind'] != 'card' or row['handler'] == 'card_dummy')):
                raise ValueError(f'changed attack zone and library fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZONE} for p in (0, 1)}
        self.pools = [ZERO, ZERO]
        self.life = [20, 20]
        self.add('source', self.name, owner, 'hand')
        self.target_ref = None
        self.target_owner = None
        if self.id == GUIDE:
            self.stage_guide()
        elif self.id == ROC:
            self.stage_roc()
        elif self.id == SKAAB:
            self.stage_skaab()
        elif self.id == MAGE:
            self.stage_mage()
        else:
            self.stage_follow_on()

    def stage_guide(self):
        owner, foe = self.owner, self.foe
        self.add('actor_deck_0', 'Forest', owner, 'library')
        self.add('actor_deck_1', 'Giant Growth', owner, 'library')
        top = {'land': ('Island', 'Lightning Bolt'), 'nonland': ('Lightning Bolt', 'Island'),
               'repeat': ('Island', 'Lightning Bolt'), 'other_attacks': ('Island', 'Lightning Bolt'),
               'empty': ()}[self.mode]
        for i, name in enumerate(top):
            self.add(f'foe_deck_{i}', name, foe, 'library')
        if self.mode == 'other_attacks':
            self.add('other_attacker', 'Grizzly Bears', owner, 'battlefield')

    def stage_roc(self):
        owner, foe = self.owner, self.foe
        self.add('own_small', 'Goblin Piker', owner, 'battlefield')
        self.add('too_tall', 'Hill Giant', foe, 'battlefield')
        if self.mode == 'no_match':
            return
        self.target_owner = owner if self.mode == 'owner_return' else foe
        self.target_ref = 'recipient'
        # Four power with one toughness is independently allowed by the rule.
        name = 'Lightning Elemental' if self.mode == 'flight' else 'Grizzly Bears'
        self.add(self.target_ref, name, self.target_owner, 'battlefield')
        if self.mode == 'owner_return':
            self.add('control', 'Control Magic', foe, 'hand')
        if self.mode == 'flight':
            self.add('reach', 'Giant Spider', foe, 'battlefield')

    def stage_skaab(self):
        owner, foe = self.owner, self.foe
        self.add('foreign_foil', 'Goblin Piker', foe, 'battlefield')
        self.add('noncreature', 'Darksteel Relic', owner, 'battlefield')
        if self.mode == 'no_other':
            return
        self.target_ref = 'recipient'
        self.target_owner = foe if self.mode == 'borrowed' else owner
        name = 'Nimble Mongoose' if self.mode == 'shroud' else 'Hill Giant'
        self.add(self.target_ref, name, self.target_owner, 'battlefield')
        if self.mode == 'borrowed':
            self.add('theft', 'Act of Treason', owner, 'hand')

    def stage_mage(self):
        owner, foe = self.owner, self.foe
        if self.mode == 'no_other':
            self.add('noncreature', 'Darksteel Relic', owner, 'battlefield')
            return
        self.target_ref = 'recipient'
        self.target_owner = foe if self.mode in ('foe', 'borrowed') else owner
        card = 'Lone Missionary' if self.mode == 'own_etb' else 'Hill Giant'
        zone = 'hand' if self.mode == 'own_etb' else 'battlefield'
        self.add(self.target_ref, card, self.target_owner, zone)
        if self.mode == 'borrowed':
            self.add('theft', 'Act of Treason', owner, 'hand')
        if self.mode == 'source_dies':
            self.add('removal', 'Murder', foe, 'hand')
        if self.mode == 'flight':
            self.add('ground', 'Grizzly Bears', foe, 'battlefield')
            self.add('reach', 'Giant Spider', foe, 'battlefield')

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

    def next_own_main(self):
        for seat in (self.foe, self.owner):
            self.emit('advance_turn')
            self.emit('untap_step', player=seat)
        self.emit('turn', player=self.owner)

    def setup_control(self):
        if self.mode not in ('borrowed', 'owner_return'):
            return
        original = self.target_ref
        name, before, _ = self.cards[original]
        controller = 1 - before
        if self.mode == 'owner_return':
            self.emit('turn', player=controller)
            self.cast('control', 'battlefield', original)
            self.emit('turn', player=self.owner)
        else:
            self.cast('theft', 'graveyard', original)
        self.move(original, 'battlefield', seat=controller)
        self.cards['borrowed'] = self.cards[original]
        self.emit('bind_permanent_copy', 'borrowed', player=controller, index=0, value=self.rows[name]['csvid'])
        self.target_ref = 'borrowed'
        self.stats(self.target_ref)

    def attack(self, attacker='source', *, reveal=False, inputs=None):
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.prop(attacker, 'attacking', 1)
        self.prop(attacker, 'tapped', 1)
        if inputs is not None:
            inputs()
        elif attacker == 'source':
            if self.id == GUIDE and reveal:
                # The public reveal uses the real single-acknowledgment dialog.
                self.emit('choose_dialog', player=self.owner, value=0)
            if self.target_ref:
                if self.id == ROC and self.owner == 0:
                    self.emit('choose_trigger_source', 'source', player=self.owner)
                if self.mode == 'decline':
                    self.emit('choose_cancel_target', player=self.owner)
                else:
                    self.emit('choose_target', self.target_ref, player=self.owner)
        self.emit('finish_attackers')
        self.emit('assert_stack', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('refresh')

    def combat(self, attacker='source', *, blocked=False):
        self.emit('begin_blockers')
        if blocked:
            ground = 'too_tall' if self.id == ROC else 'ground'
            self.emit('block', ground, other=attacker, value=0)
            self.emit('block', 'reach', other=attacker, value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_unused_choices', value=0)
        if blocked:
            self.stats(attacker, damage=2)
            self.stats('reach', damage=3)
        else:
            self.life[self.foe] -= int(self.oracle[self.cards[attacker][0]]['power'])

    def receipts(self, *, copied_source=None):
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.mana(player)
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=0)
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                if copied_source is not None and player == self.owner and zone == 'battlefield':
                    counts[self.name] -= 1
                    counts[copied_source] += 1
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])
                if zone == 'library':
                    for index, name in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index,
                                  value=self.rows[name]['csvid'])

    def guide(self):
        # Haste allows the actual source to turn sideways on the casting turn.
        self.prop('source', 'sick', 0)
        attacker = 'other_attacker' if self.mode == 'other_attacks' else 'source'
        repeats = 2 if self.mode == 'repeat' else 1
        for index in range(repeats):
            if index:
                self.next_own_main()
            deck = self.zones[self.foe]['library']
            reveal = attacker == 'source' and bool(deck)
            self.attack(attacker, reveal=reveal)
            if reveal and 'Land' in self.oracle[deck[0]]['type_line']:
                card, = [ref for ref, item in self.cards.items() if item == (deck[0], self.foe, 'library')]
                self.move(card, 'hand')
            self.receipts()
            self.combat(attacker)

    def bounce(self):
        self.next_own_main()
        self.setup_control()
        attacker = 'recipient' if self.mode == 'other_attacks' else 'source'
        self.attack(attacker)
        if self.target_ref and attacker == 'source' and self.mode != 'decline':
            self.move(self.target_ref, 'hand', seat=self.target_owner)
            if 'control' in self.cards:
                self.move('control', 'graveyard')
        self.receipts()
        self.combat(attacker, blocked=self.mode == 'flight')

    def blink(self):
        self.next_own_main()
        self.setup_control()
        if self.mode == 'own_etb':
            self.cast('recipient', 'battlefield')
            self.life[self.owner] += 4
            self.receipts()
        self.attack()
        if self.target_ref:
            self.move(self.target_ref, 'exile', seat=self.target_owner)
        self.receipts()
        self.combat(blocked=self.mode == 'flight')
        if self.mode == 'source_dies':
            self.cast('removal', 'graveyard', 'source')
            self.move('source', 'graveyard')
        self.receipts()
        # This established driver runs real cleanup and the production end-turn
        # trigger used by remove_until_eot(), without a simulated zone change.
        self.emit('cleanup')
        self.emit('refresh')
        self.emit('assert_unused_choices', value=0)
        if self.target_ref:
            self.move(self.target_ref, 'battlefield', seat=self.target_owner)
        if self.mode == 'own_etb':
            self.life[self.owner] += 4

    def stage_follow_on(self):
        mode, owner, foe = self.mode, self.owner, self.foe
        if self.id == WHALE:
            self.target_owner = owner if mode == 'owner_return' else foe
            self.target_ref = 'recipient'
            self.add('recipient', 'Walking Corpse', self.target_owner, 'battlefield')
            self.add('blocker', 'Hill Giant', foe, 'battlefield')
            if mode == 'islandwalk':
                self.add('land', 'Island', foe, 'battlefield')
            if mode == 'owner_return':
                self.add('control', 'Control Magic', foe, 'hand')
            if mode in ('leaves', 'owner_return'):
                self.add('removal', 'Murder', foe, 'hand')
        elif self.id == SCUTTLER:
            self.add('nonartifact', 'Grizzly Bears', owner, 'battlefield')
            self.add('foreign_artifact', 'Mind Stone', foe, 'battlefield')
            if mode != 'no_artifact':
                self.target_owner = foe if mode == 'borrowed' else owner
                self.target_ref = 'recipient'
                self.add('recipient', 'Ornithopter' if mode == 'borrowed' else 'Darksteel Relic',
                         self.target_owner, 'battlefield')
            if mode == 'borrowed':
                self.add('theft', 'Act of Treason', owner, 'hand')
        elif self.id in (SENTINEL, THRULL):
            self.add('grave_land', 'Swamp', owner, 'graveyard')
            self.add('grave_spell', 'Unsummon', owner, 'graveyard')
            self.add('foreign_card', 'Fervor', foe, 'graveyard')
            if mode != 'wrong_owner':
                name = {'artifact': 'Mind Stone', 'creature': 'Grizzly Bears'}.get(mode, 'Glorious Anthem')
                self.add('recipient', name, owner, 'graveyard')
            if mode == 'shroud':
                self.add('shroud', 'Silent Gravestone', foe, 'battlefield')
            if mode == 'flight':
                self.add('ground', 'Hill Giant', foe, 'battlefield')
                self.add('reach', 'Giant Spider', foe, 'battlefield')
            if mode.startswith('extort_'):
                self.add('spell', 'Expedition Map', owner, 'hand')
        elif self.id == THIEF:
            if mode in ('copy', 'copy_flying', 'decline'):
                self.target_owner = owner if mode == 'copy_flying' else foe
                self.target_ref = 'recipient'
                self.add('recipient', 'Wind Drake' if mode == 'copy_flying' else 'Hill Giant',
                         self.target_owner, 'battlefield')
            if mode == 'token_only':
                self.add('token_spell', 'Raise the Alarm', owner, 'hand')
            if mode == 'copy_flying':
                self.add('ground', 'Grizzly Bears', foe, 'battlefield')
        elif self.id == BANE:
            self.add('protected_own', 'Grizzly Bears', owner, 'battlefield')
            if mode == 'planeswalker':
                self.add('first', 'Jace Beleren', foe, 'battlefield')
            elif mode != 'empty':
                self.add('first', 'Forest', foe, 'battlefield')
            if mode == 'two':
                self.add('second', 'Darksteel Relic', foe, 'battlefield')
                self.add('spared', 'Hill Giant', foe, 'battlefield')
        elif self.id == SAGE:
            self.target_owner = owner if mode == 'own' else foe
            self.target_ref = 'recipient'
            self.add('recipient', 'Lightning Elemental' if mode in ('equal', 'prowess') else 'Hill Giant',
                     self.target_owner, 'battlefield')
            if mode == 'prowess':
                self.add('spell', 'Expedition Map', owner, 'hand')
        elif self.id == SPHINX:
            self.stage_sphinx()
        elif self.id == DRAGON:
            self.add('hand_card', 'Hill Giant', owner, 'hand')
            top = 'Lightning Bolt' if mode == 'discard_drawn' else 'Island'
            self.add('first_draw', top, owner, 'library')
            self.add('second_draw', 'Forest', owner, 'library')
            self.add('foreign_deck', 'Mountain', foe, 'library')
            if mode == 'flight':
                self.add('ground', 'Grizzly Bears', foe, 'battlefield')
                self.add('reach', 'Giant Spider', foe, 'battlefield')
        else:
            self.add('first_exile', 'Forest', owner, 'library')
            self.add('second_exile', 'Lightning Bolt', owner, 'library')
            self.add('last_library', 'Island', owner, 'library')
            self.add('unlinked_exile', 'Plains', owner, 'exile')
            self.add('discard_0', 'Mountain', owner, 'hand')
            self.add('discard_1', 'Swamp', owner, 'hand')
            self.add('foreign_deck', 'Island', foe, 'library')
            if mode == 'died':
                self.add('removal', 'Murder', foe, 'hand')
            if mode == 'two_couriers':
                self.add('other_courier', self.name, owner, 'hand')

    def plan_optional_trigger(self):
        if self.id in OPTIONAL_TRIGGER and self.owner == 0:
            self.emit('choose_trigger_source', 'source', player=self.owner)

    def plan_creature_or_artifact(self):
        if not self.target_ref:
            return
        self.plan_optional_trigger()
        if self.mode == 'decline':
            self.emit('choose_cancel_target', player=self.owner)
        else:
            self.emit('choose_target', self.target_ref, player=self.owner)

    def finish_follower_combat(self, damage_to_player, *, blocks=(), attacker='source'):
        self.emit('begin_blockers')
        for ref, allowed in blocks:
            self.emit('block', ref, other=attacker, value=int(allowed))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_unused_choices', value=0)
        self.life[self.foe] -= damage_to_player

    def whale(self):
        self.setup_control()
        self.attack(inputs=self.plan_creature_or_artifact)
        exiled = self.mode != 'decline'
        if exiled:
            self.move(self.target_ref, 'exile', seat=self.target_owner)
            if 'control' in self.cards:
                self.move('control', 'graveyard')
        self.receipts()
        blocked = self.mode == 'no_island'
        blocks = (('blocker', blocked),) if self.mode in ('islandwalk', 'no_island') else ()
        self.finish_follower_combat(0 if blocked else 5, blocks=blocks)
        if blocked:
            self.move('blocker', 'graveyard')
            self.stats('source', damage=3)
        if self.mode in ('leaves', 'owner_return'):
            self.cast('removal', 'graveyard', 'source')
            self.move('source', 'graveyard')
            self.move(self.target_ref, 'battlefield', seat=self.target_owner)
        else:
            self.emit('cleanup')
            self.emit('refresh')

    def scuttler(self):
        self.setup_control()
        self.attack(inputs=self.plan_creature_or_artifact)
        if self.target_ref:
            self.move(self.target_ref, 'hand', seat=self.target_owner)
        self.receipts()
        self.finish_follower_combat(4)

    def extort(self):
        fee = {'extort_black': 1, 'extort_white': 5}.get(self.mode)
        funds = [0] * 6
        funds[0] = 1
        if fee is not None:
            funds[fee] = 1
        self.mana(self.owner, funds, write=True)
        self.emit('refresh')
        self.emit('cast', 'spell', value=1)
        self.emit('assert_stack', value=1)
        self.prop('spell', 'in_hand', 0)
        funds[0] = 0
        self.mana(self.owner, funds)
        if fee is not None and self.owner == 0:
            self.emit('choose_trigger_source', 'source', player=self.owner)
        self.emit('announce_spell', 'spell')
        self.emit('assert_unused_choices', value=0)
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.move('spell', 'battlefield')
        if fee is not None:
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
        self.receipts()

    def grave_reward(self):
        if self.mode.startswith('extort_'):
            self.extort()
        available = self.mode not in ('shroud', 'wrong_owner')
        def inputs():
            if available:
                self.plan_optional_trigger()
                if self.mode == 'decline':
                    self.emit('choose_zone', player=self.owner, value=-1)
                else:
                    name = self.cards['recipient'][0]
                    self.emit('observe_search_filters', value=1)
                    self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid'])
        self.attack(inputs=inputs)
        recovered = available and self.mode != 'decline'
        if recovered:
            self.move('recipient', 'battlefield' if self.id == SENTINEL else 'hand')
            for ref, legal in (('recipient', 1), ('grave_land', 0), ('grave_spell', 0)):
                self.emit('assert_offered_legal_card', index=self.rows[self.cards[ref][0]]['csvid'], value=legal)
        self.receipts()
        damage = 5 if self.id == SENTINEL and recovered else 4
        blocks = (('ground', False), ('reach', True)) if self.mode == 'flight' else ()
        self.finish_follower_combat(0 if blocks else damage, blocks=blocks)
        if blocks:
            self.move('reach', 'graveyard')
            self.prop('source', 'damage', 2)

    def identity(self):
        if self.mode == 'token_only':
            self.cast('token_spell', 'graveyard')
            for index in range(2):
                self.cards[f'actual_token_{index}'] = 'Soldier', self.owner, 'battlefield'
                self.zones[self.owner]['battlefield'].append('Soldier')
        self.attack(inputs=self.plan_creature_or_artifact)
        copied = bool(self.target_ref) and self.mode != 'decline'
        damage = 0
        copied_name = None
        if copied:
            name = copied_name = self.cards[self.target_ref][0]
            self.move(self.target_ref, 'exile', seat=self.target_owner)
            self.prop('source', 'csvid', self.rows[name]['csvid'])
            for field in ('power', 'toughness'):
                self.prop('source', field, int(self.oracle[name][field]))
            damage = int(self.oracle[name]['power'])
        self.receipts(copied_source=copied_name)
        blocks = (('ground', False),) if self.mode == 'copy_flying' else ()
        self.finish_follower_combat(damage, blocks=blocks)
        self.receipts(copied_source=copied_name)
        self.emit('cleanup')
        self.emit('refresh')
        if copied:
            self.move(self.target_ref, 'battlefield', seat=self.target_owner)
        self.stats('source')

    def bane(self):
        targets = [ref for ref in ('first', 'second') if ref in self.cards]
        def inputs():
            for ref in targets:
                self.emit('choose_target', ref, player=self.foe)
        self.attack(inputs=inputs)
        for ref in targets:
            self.move(ref, 'exile')
        self.receipts()
        self.finish_follower_combat(7)

    def sage(self):
        boosted = self.mode == 'prowess'
        if boosted:
            self.cast('spell', 'battlefield')
            self.prop('source', 'power', 5)
            self.prop('source', 'toughness', 6)
        self.attack(inputs=self.plan_creature_or_artifact)
        if self.mode not in ('equal', 'decline'):
            self.move(self.target_ref, 'hand', seat=self.target_owner)
        self.receipts()
        self.finish_follower_combat(5 if boosted else 4)
        self.emit('cleanup')
        self.emit('refresh')
        self.stats('source')

    def dragon(self):
        discarded = 'hand_card' if self.mode == 'large_discard' else 'first_draw'
        name = self.cards[discarded][0]
        self.attack(inputs=lambda: self.emit('choose_zone_card', player=self.owner, value=self.rows[name]['csvid']))
        self.move('first_draw', 'hand')
        self.move(discarded, 'graveyard')
        power = 3 + int(self.oracle[name]['cmc'])
        self.prop('source', 'power', power)
        self.prop('source', 'toughness', 5)
        self.receipts()
        blocks = (('ground', False), ('reach', True)) if self.mode == 'flight' else ()
        self.finish_follower_combat(0 if blocks else power, blocks=blocks)
        if blocks:
            self.prop('source', 'damage', 2)
            self.prop('reach', 'damage', 3)
        self.emit('cleanup')
        self.emit('refresh')
        self.stats('source')

    def courier(self):
        self.prop('source', 'sick', 0)
        self.attack()
        self.move('first_exile', 'exile')
        self.receipts()
        self.finish_follower_combat(1)
        recovered = ['first_exile']
        if self.mode in ('repeat', 'two_couriers'):
            twice = self.mode == 'repeat'
            attacker = 'source' if twice else 'other_courier'
            if not twice:
                self.cast(attacker, 'battlefield')
            self.next_own_main()
            self.attack(attacker)
            self.move('second_exile', 'exile')
            self.receipts()
            self.finish_follower_combat(1, attacker=attacker)
            if twice:
                recovered.append('second_exile')
        if self.mode == 'died':
            self.cast('removal', 'graveyard', 'source')
            self.move('source', 'graveyard')
            return
        if self.mode == 'short':
            self.emit('can_activate', 'source', value=0)
            self.emit('activate', 'source', value=0)
            return
        self.mana(self.owner, (0, 0, 0, 0, 1, 0), write=True)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        for ref in ('discard_0', 'discard_1'):
            if self.owner == 0:
                self.emit('choose_zone_card', player=0, value=self.rows[self.cards[ref][0]]['csvid'])
            else:
                self.emit('choose_dialog', player=1, value=0)
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.move('source', 'graveyard')
        for ref in ('discard_0', 'discard_1'):
            self.move(ref, 'graveyard')
        self.receipts_before_courier_return(recovered)
        self.emit('resolve')
        self.emit('refresh')
        for ref in recovered:
            self.move(ref, 'hand')

    def receipts_before_courier_return(self, recovered):
        self.mana(self.owner)
        self.emit('assert_zone', player=self.owner, zone='hand', value=0)
        for ref in recovered:
            self.emit('assert_zone_multiplicity', player=self.owner, zone='exile',
                      index=self.rows[self.cards[ref][0]]['csvid'], value=1)

    def stage_sphinx(self):
        decks = (('Lightning Bolt', 'Plains', 'Forest'), ('Island', 'Mountain', 'Unsummon'))
        for seat, deck in enumerate(decks):
            for index, name in enumerate(deck):
                self.add(f'sphinx_{seat}_{index}', name, seat, 'library')
        self.add('hand_foil', 'Swamp', 0, 'hand')
        if self.mode == 'other_attacks':
            self.add('other_attacker', 'Goblin Piker', self.owner, 'battlefield')
        if self.mode == 'flight':
            self.add('ground', 'Grizzly Bears', self.foe, 'battlefield')
            self.add('reach', 'Giant Spider', self.foe, 'battlefield')

    def sphinx_start(self):
        self.emit('assert_card_available', index=SPHINX, value=1)
        # The production AI guess samples one library slot at 6 life.
        self.life[1] = 6
        self.emit('set_life', player=1, value=6)
        if self.mode != 'short_fee':
            return True
        budget = (1, 0, 2, 0, 0, 0)
        self.mana(self.owner, budget, write=True)
        self.emit('refresh')
        self.emit('cast', 'source', value=0)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)
        self.prop('source', 'in_hand', 1)
        self.mana(self.owner, budget)
        self.mana(self.owner, write=True)
        self.receipts()
        return False

    def sphinx(self):
        other = self.mode == 'other_attacks'
        attacker = 'other_attacker' if other else 'source'
        if other:
            self.attack(attacker)
        else:
            guessed = 'Hill Giant' if self.mode == 'miss' else 'Lightning Bolt'
            def inputs():
                # The production handler handles the attacker's side first.
                # Both reveal notices are presented to the human seat.
                if self.owner == 1:
                    self.emit('choose_dialog', player=0, value=0)
                self.emit('choose_card_name', player=0, value=self.rows[guessed]['csvid'])
                self.emit('choose_dialog', player=0, value=0)
                if self.owner == 0:
                    self.emit('choose_dialog', player=0, value=0)
            self.attack(inputs=inputs)
            self.move('sphinx_1_0', 'hand')
            if self.mode == 'miss':
                library = self.zones[0]['library']
                library.append(library.pop(0))
            else:
                self.move('sphinx_0_0', 'hand')
        self.receipts()
        flying = self.mode == 'flight'
        blocks = (('ground', False), ('reach', True)) if flying else ()
        self.finish_follower_combat(0 if flying else 2 if other else 4, blocks=blocks, attacker=attacker)
        if flying:
            self.move('reach', 'graveyard')
            self.stats('source', damage=2)

    def follow_on(self):
        if self.id != BOMAT:
            self.next_own_main()
        run = {WHALE: self.whale, SCUTTLER: self.scuttler, SENTINEL: self.grave_reward,
               THRULL: self.grave_reward, THIEF: self.identity, BANE: self.bane,
               SAGE: self.sage, SPHINX: self.sphinx, DRAGON: self.dragon, BOMAT: self.courier}
        run[self.id]()


    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, write=True)
        if self.id == SPHINX and not self.sphinx_start():
            self.emit('present_ai_dialogs', value=0)
            return
        self.cast('source', 'battlefield')
        self.stats('source')
        if self.id == GUIDE:
            self.guide()
        elif self.id in (ROC, SKAAB):
            self.bounce()
        elif self.id == MAGE:
            self.blink()
        else:
            self.follow_on()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('attack zone and library source CSVID changed')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        gaps = {
            BOMAT: ['The tested zone observer cannot distinguish public and face-down exile visibility.'],
            SPHINX: ['The scripted callback cannot observe both names being committed before either reveal.'],
        }.get(csvid, [])
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=gaps)))
    return cases
