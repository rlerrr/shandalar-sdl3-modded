"""Paid Flash Auras, entry counters, combat modifiers, and damage replacement."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step


FAMILY = 'flash_remaining_auras_and_enchantments'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
FLASH = 'Flash (You may cast this spell any time you could cast an instant.)'
ENCHANT = 'Enchant creature'
WILL, TALONS, ISOLATION, MUTATION = 7251, 12274, 8696, 16195
MAGGOT, MELTDOWN, HOWLPACK, DICTATE = 4526, 16271, 15856, 13875
MODELS = {
    MAGGOT: ('{2}{B}', ('Flash', ENCHANT, 'Enchanted creature gets +2/-2.'),
             ('lethal_attack', 'stat_delta', 'remove_aura', 'lost_target')),
    MELTDOWN: ('{1}{U}', (FLASH, 'Enchant creature or Vehicle',
                         'When this Aura enters, you get {E}{E} (two energy counters).',
                         'Enchanted creature gets -4/-0.'),
               ('creature_energy', 'vehicle_energy', 'remove_aura', 'lost_target')),
    HOWLPACK: ('{2}{G}', ('Flash', "Each creature you control that's a Wolf or a Werewolf gets +1/+1 and has trample."),
               ('team_scope', 'team_entry', 'team_trample', 'team_remove')),
    DICTATE: ('{3}{R}{R}', ('Flash', 'If a source would deal damage to a permanent or player, '
                             'it deals double that damage to that permanent or player instead.'),
              ('combat_player', 'combat_both', 'direct_damage', 'remove_before_damage')),
    WILL: ('{1}{W}', (FLASH, ENCHANT, 'Enchanted creature gets +1/+2.'),
           ('own_creature', 'foe_creature', 'rescue_blocker', 'remove_aura', 'lost_target')),
    TALONS: ('{1}{R}', (FLASH, ENCHANT, 'Enchanted creature has "{1}{R}: This creature gets +2/+0 until end of turn."'),
             ('paid_pump', 'foe_pump', 'short_pump', 'remove_after_pump', 'lost_target')),
    ISOLATION: ('{1}{W}', ('Flash', ENCHANT,
                          'Enchanted creature has shadow. (It can block or be blocked by only creatures with shadow.)',
                          'Prevent all damage that would be dealt by enchanted creature.'),
                ('shadow_and_prevent', 'shadow_blocks', 'noncombat_prevent', 'removed_in_combat', 'lost_target')),
    MUTATION: ('{U}', ('Flash', ENCHANT, 'Enchanted creature gets -X/-0, where X is the number of cards in your graveyard.'),
               ('live_graveyard', 'foe_graveyard_only', 'negative_combat', 'lost_target')),
}
PARENT = "Traveler's Amulet"
BODY = 'Goblin Balloon Brigade'
ATTACKER = 'Goblin Tinkerer'
SHADOW = 'Soltari Foot Soldier'
PINGER = 'Prodigal Sorcerer'
DRAW = 'Reach Through Mists'
RETURN = 'Unsummon'
REMOVE = 'Disenchant'
COMBAT = {'rescue_blocker', 'shadow_and_prevent', 'shadow_blocks', 'removed_in_combat', 'negative_combat'}
REMOVAL = {'remove_aura', 'remove_after_pump', 'removed_in_combat'}
WOLF = 'Darkthicket Wolf'
WEREWOLF = 'Kessig Prowler'
WALL = 'Wall of Roots'
VEHICLE = 'Sky Skiff'
SHOCK = 'Shock'
FOLLOWERS = {MAGGOT, MELTDOWN, HOWLPACK, DICTATE}
WITNESSES = {PARENT, BODY, ATTACKER, SHADOW, PINGER, DRAW, RETURN, REMOVE, 'Forest',
             'Island', 'Mountain', 'Plains', WOLF, WEREWOLF, WALL, VEHICLE, SHOCK}
NEW_COMBAT = {'lethal_attack', 'combat_player', 'combat_both'}
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
UNTAPPED = 8
ZERO = (0,) * 6
SEED = 2739
PUMP_PRICE = (1, 0, 0, 0, 1, 0)


class AuraCase:
    def __init__(self, record, csvid, owner, mode, rows, oracle):
        self.record, self.csvid, self.owner, self.foe, self.mode = record, csvid, owner, 1-owner, mode
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps = {}, [], []
        self.pools, self.life = [ZERO, ZERO], [20, 20]
        self.attached, self.pumped = False, 0
        self.add('source', record['name'], owner, 'hand')
        self.build()

    def build(self):
        mode, csvid, owner = self.mode, self.csvid, self.owner
        if mode in COMBAT:
            self.add('attacker', ATTACKER, self.foe)
            if mode != 'negative_combat':
                self.add('blocker', SHADOW if mode == 'shadow_blocks' else BODY, owner)
            self.host = 'blocker' if mode == 'rescue_blocker' else 'attacker'
        else:
            self.host = 'host'
            hostile = csvid in (ISOLATION, MUTATION) or mode in ('foe_creature', 'foe_pump')
            self.add('host', PINGER if mode == 'noncombat_prevent' else BODY, self.foe if hostile else owner)
            self.add('bystander', BODY, owner if hostile else self.foe)
            if mode != 'lost_target':
                self.add('parent', PARENT, self.foe, 'hand')
        if mode == 'lost_target':
            self.add('answer', RETURN, self.foe, 'hand')
        elif mode in REMOVAL:
            self.add('answer', REMOVE, self.foe, 'hand')
        if csvid == MUTATION and mode != 'lost_target':
            self.add('foe_grave_a', 'Island', self.foe, 'graveyard')
            self.add('foe_grave_b', 'Plains', self.foe, 'graveyard')
            if mode != 'foe_graveyard_only':
                self.add('own_grave', 'Mountain', owner, 'graveyard')
            if mode == 'negative_combat':
                self.add('own_grave_b', 'Forest', owner, 'graveyard')
            if mode == 'live_graveyard':
                self.add('draw_spell', DRAW, owner, 'hand')
                self.add('drawn', 'Forest', owner, 'library')

    def add(self, ref, name, player, zone='battlefield'):
        entry = card(name, ref, player, zone)
        if zone == 'battlefield':
            entry['state'] = ['untapped']
        self.setup.append(entry)
        record = self.oracle[name]
        printed = tuple(int(record[prop]) for prop in ('power', 'toughness')) if 'power' in record else None
        self.cards[ref] = dict(name=name, player=player, zone=zone, tapped=False, damage=0, printed=printed)

    def emit(self, op, ref=None, other=None, prop=None, *, player=None, value=None, zone=None, index=None, color=None):
        fields = dict(ref=ref, other=other, property=prop, player=player,
                      value=value, zone=zone, index=index, color=color)
        self.steps.append(step(op, **{key: value for key, value in fields.items() if value is not None}))

    def property(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=int(value))

    def move(self, ref, zone):
        self.cards[ref].update(zone=zone, tapped=False, damage=0)

    def stats(self, ref):
        power, toughness = self.cards[ref]['printed']
        if ref != self.host:
            return power, toughness
        if self.attached and self.csvid == WILL:
            power, toughness = power+1, toughness+2
        elif self.attached and self.csvid == MUTATION:
            power -= sum(c['zone'] == 'graveyard' and c['player'] == self.owner for c in self.cards.values())
        return power + self.pumped, toughness

    def fund(self, player, pool):
        self.pools[player] = tuple(pool)
        for color, value in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=value)
        self.emit('refresh')

    def spend(self, player, cost):
        self.pools[player] = tuple(have-due for have, due in zip(self.pools[player], cost))
        assert min(self.pools[player]) >= 0

    def observe_mana(self):
        for player, pool in enumerate(self.pools):
            for color, value in enumerate(pool):
                self.emit('assert_mana', player=player, color=color, value=value)

    def observe_creatures(self):
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            self.property(ref, 'in_play', 1)
            self.property(ref, 'tapped', card['tapped'])
            if card['printed'] is not None:
                for key, number in zip(('power', 'toughness'), self.stats(ref)):
                    self.property(ref, key, number)
                self.property(ref, 'damage', card['damage'])

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.observe_mana()
        self.observe_creatures()
        names = sorted({card['name'] for card in self.cards.values()})
        for player in (0, 1):
            self.emit('assert_life', player=player, value=self.life[player])
            for zone in ZONES:
                contents = [card['name'] for card in self.cards.values() if card['player'] == player and card['zone'] == zone]
                self.emit('assert_zone', player=player, zone=zone, value=len(contents))
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone, value=contents.count(name),
                              index=self.rows[name]['csvid'])

    def cast(self, ref, *, target=None, response=False):
        entry = self.cards[ref]
        actor = entry['player']
        cost = cost_from_oracle(self.oracle[entry['name']])
        self.fund(actor, cost)
        if type(target) is int:
            self.emit('choose_player', player=actor, value=target)
        elif target:
            self.emit('choose_target', target, player=actor)
        self.emit('cast_response' if response else 'cast', ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=2 if response else 1)
        self.spend(actor, cost)
        self.observe_mana()
        self.move(ref, 'pending')

    def drain(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')

    def enchant(self):
        response = 'parent' in self.cards
        if response:
            self.cast('parent')
            self.emit('announce_spell', 'parent')
        self.cast('source', target=self.host, response=response)
        self.property('source', 'in_play', 0)
        self.observe_creatures()
        if self.mode == 'lost_target':
            self.emit('announce_spell', 'source')
            self.cast('answer', target=self.host, response=True)
            self.drain(response=True)
            self.move(self.host, 'hand')
            self.move('answer', 'graveyard')
            self.move('source', 'graveyard')
        else:
            self.drain(response=response)
            self.move('source', 'battlefield')
            self.attached = True
            if response:
                self.move('parent', 'battlefield')
        self.receipt()

    def disenchant(self):
        self.cast('answer', target='source')
        self.drain()
        self.move('answer', 'graveyard')
        self.move('source', 'graveyard')
        self.attached = False
        self.receipt()

    def talons(self):
        host_actor = self.cards[self.host]['player']
        mode = self.mode
        budget = PUMP_PRICE if mode != 'short_pump' else (1, 0, 0, 0, 0, 0)
        for _ in range(2 if mode == 'paid_pump' else 1):
            self.fund(host_actor, budget)
            legal = mode != 'short_pump'
            self.emit('can_activate', 'source', value=int(legal))
            self.emit('activate', 'source', value=int(legal))
            self.emit('assert_choices_consumed')
            self.emit('assert_stack', value=int(legal))
            if legal:
                self.spend(host_actor, PUMP_PRICE)
                self.observe_mana()
                self.property(self.host, 'power', self.stats(self.host)[0])
                self.drain()
                self.pumped += 2
            self.receipt()
        if mode == 'remove_after_pump':
            self.disenchant()
        if mode != 'short_pump':
            self.emit('advance_turn')
            self.emit('refresh')
            self.pumped = 0
            self.receipt()

    def noncombat(self):
        self.emit('choose_player', player=self.foe, value=self.owner)
        self.emit('activate', self.host, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards[self.host]['tapped'] = True
        self.drain()
        self.receipt()

    def grow_graveyard(self):
        self.cast('draw_spell')
        self.drain()
        self.move('draw_spell', 'graveyard')
        self.move('drawn', 'hand')
        self.receipt()

    def combat_start(self):
        self.emit('begin_attackers')
        self.emit('attack', 'attacker', value=1)
        self.emit('finish_attackers')
        self.cards['attacker']['tapped'] = True
        if self.mode == 'rescue_blocker':
            self.emit('begin_blockers')
            self.emit('block', 'blocker', 'attacker', value=1)
            self.emit('advance_combat')

    def combat_finish(self):
        mode = self.mode
        if mode != 'rescue_blocker':
            self.emit('begin_blockers')
            if 'blocker' in self.cards:
                self.emit('block', 'blocker', 'attacker', value=int(mode != 'shadow_and_prevent'))
            self.emit('advance_combat')
        blocked = mode in ('rescue_blocker', 'shadow_blocks', 'removed_in_combat')
        self.property('attacker', 'blocked', blocked)
        self.emit('combat_damage')
        self.drain()
        self.emit('end_combat')
        if mode == 'rescue_blocker':
            self.cards['blocker']['damage'] = 1
            self.move('attacker', 'graveyard')
        elif mode in ('shadow_blocks', 'removed_in_combat'):
            self.cards['attacker']['damage'] = 1
            if mode == 'removed_in_combat':
                self.move('blocker', 'graveyard')
        self.receipt()

    def play(self):
        self.emit('turn', player=self.foe)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        if self.mode in COMBAT:
            self.combat_start()
        self.enchant()
        if self.mode == 'lost_target':
            return
        if self.csvid == TALONS:
            self.talons()
        elif self.mode in REMOVAL:
            self.disenchant()
        elif self.mode == 'noncombat_prevent':
            self.noncombat()
        elif self.mode == 'live_graveyard':
            self.grow_graveyard()
        if self.mode in COMBAT:
            self.combat_finish()


class FollowerCase(AuraCase):
    def __init__(self, *args):
        self.energy = 0
        super().__init__(*args)

    def build(self):
        mode = self.mode
        self.host = 'host'
        if self.csvid in (MAGGOT, MELTDOWN):
            if mode == 'lethal_attack':
                self.host = 'attacker'
                self.add(self.host, ATTACKER, self.foe)
            elif mode == 'vehicle_energy':
                self.add(self.host, VEHICLE, self.foe)
                # An uncrewed Vehicle has printed P/T without being a creature.
                self.cards[self.host]['printed'] = None
                self.add('pilot', BODY, self.foe)
            else:
                name, controller = (WALL, self.owner) if self.csvid == MAGGOT and mode != 'lost_target' else (BODY, self.foe)
                self.add(self.host, name, controller)
            if mode == 'lost_target':
                self.add('answer', RETURN, self.foe, 'hand')
            elif mode != 'lethal_attack':
                self.add('parent', PARENT, self.foe, 'hand')
        elif self.csvid == HOWLPACK:
            self.add('parent', PARENT, self.foe, 'hand')
            self.add('wolf', WOLF, self.owner, 'hand' if mode == 'team_entry' else 'battlefield')
            self.add('foe_wolf', WOLF, self.foe)
            self.add('own_other', BODY, self.owner)
            self.add('foe_other', BODY, self.foe)
            if mode in ('team_scope', 'team_entry'):
                for ref, actor in (('werewolf', self.owner), ('foe_werewolf', self.foe)):
                    self.add(ref, WEREWOLF, actor)
                    face = self.oracle[WEREWOLF]['card_faces'][0]
                    self.cards[ref]['printed'] = tuple(int(face[k]) for k in ('power', 'toughness'))
        else:
            if mode in NEW_COMBAT:
                self.add('attacker', ATTACKER, self.foe)
                if mode == 'combat_both':
                    self.add('blocker', ATTACKER, self.owner)
            else:
                self.add('parent', PARENT, self.foe, 'hand')
                self.add('durable', WALL, self.owner)
                self.add('foe_shock', SHOCK, self.foe, 'hand')
                if mode == 'direct_damage':
                    self.add('own_shock', SHOCK, self.owner, 'hand')
        if mode in ('remove_aura', 'team_remove', 'remove_before_damage'):
            self.add('answer', REMOVE, self.foe, 'hand')

    def stats(self, ref):
        power, toughness = self.cards[ref]['printed']
        if self.attached and ref == self.host:
            if self.csvid == MAGGOT:
                return power + 2, toughness - 2
            if self.csvid == MELTDOWN:
                return power - 4, toughness
        if (self.csvid == HOWLPACK and self.cards['source']['zone'] == 'battlefield'
                and self.cards[ref]['player'] == self.owner and self.cards[ref]['name'] in (WOLF, WEREWOLF)):
            return power + 1, toughness + 1
        return power, toughness

    def receipt(self):
        super().receipt()
        if self.csvid == MELTDOWN:
            self.observe_energy()

    def observe_energy(self):
        for player in (0, 1):
            self.emit('assert_energy', player=player, value=self.energy if player == self.owner else 0)

    def cast_follower(self):
        response = 'parent' in self.cards
        aura = self.csvid in (MAGGOT, MELTDOWN)
        if response:
            self.cast('parent')
            self.emit('announce_spell', 'parent')
        self.cast('source', target=self.host if aura else None, response=response)
        self.property('source', 'in_play', 0)
        self.observe_creatures()
        if self.csvid == MELTDOWN:
            self.observe_energy()
        if self.mode == 'lost_target':
            self.emit('announce_spell', 'source')
            self.cast('answer', target=self.host, response=True)
            self.drain(response=True)
            self.move(self.host, 'hand')
            self.move('answer', 'graveyard')
            self.move('source', 'graveyard')
        else:
            self.drain(response=response)
            self.move('source', 'battlefield')
            if response:
                self.move('parent', 'battlefield')
            self.attached = aura
            if self.csvid == MELTDOWN:
                self.energy += 2
            if self.mode == 'lethal_attack':
                self.move(self.host, 'graveyard')
                self.move('source', 'graveyard')
                self.attached = False
        self.receipt()

    def crew(self):
        self.emit('assert_matching_permanents', player=self.foe, prop='artifact',
                  index=self.rows[VEHICLE]['csvid'], value=1)
        self.emit('can_activate', self.host, value=1)
        self.emit('choose_target', 'pilot', player=self.foe)
        self.emit('activate', self.host, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=1)
        self.cards['pilot']['tapped'] = True
        self.property('pilot', 'tapped', 1)
        self.property(self.host, 'tapped', 0)
        self.drain()
        record = self.oracle[VEHICLE]
        self.cards[self.host]['printed'] = tuple(int(record[k]) for k in ('power', 'toughness'))
        self.receipt()

    def howlpack(self):
        mode = self.mode
        if mode == 'team_scope':
            return
        if mode == 'team_remove':
            self.disenchant()
        self.emit('advance_turn')
        self.emit('untap_step', player=self.owner)
        self.emit('upkeep_step', player=self.owner)
        self.emit('turn', player=self.owner)
        if mode == 'team_entry':
            self.cast('wolf')
            self.drain()
            self.move('wolf', 'battlefield')
            self.receipt()
            return
        self.emit('begin_attackers')
        self.emit('attack', 'wolf', value=1)
        self.emit('finish_attackers')
        self.cards['wolf']['tapped'] = True
        self.emit('begin_blockers')
        self.emit('block', 'foe_other', 'wolf', value=1)
        self.emit('advance_combat')
        self.property('wolf', 'blocked', 1)
        self.emit('combat_damage')
        self.drain()
        self.emit('end_combat')
        self.cards['wolf']['damage'] = 1
        self.move('foe_other', 'graveyard')
        self.life[self.foe] -= 2 if mode == 'team_trample' else 0
        self.receipt()

    def dictate(self):
        mode = self.mode
        if mode == 'remove_before_damage':
            self.disenchant()
        if mode not in NEW_COMBAT:
            doubled = self.cards['source']['zone'] == 'battlefield'
            self.cast('foe_shock', target='durable')
            self.drain()
            self.move('foe_shock', 'graveyard')
            self.cards['durable']['damage'] = 4 if doubled else 2
            self.receipt()
            if mode == 'direct_damage':
                self.cast('own_shock', target=self.foe)
                self.drain()
                self.move('own_shock', 'graveyard')
                self.life[self.foe] -= 4
                self.receipt()
            return
        self.emit('begin_blockers')
        blocked = mode == 'combat_both'
        if blocked:
            self.emit('block', 'blocker', 'attacker', value=1)
        self.emit('advance_combat')
        self.property('attacker', 'blocked', blocked)
        self.emit('combat_damage')
        self.drain()
        self.emit('end_combat')
        if blocked:
            self.move('attacker', 'graveyard')
            self.move('blocker', 'graveyard')
        else:
            self.life[self.owner] -= 2
        self.receipt()

    def play(self):
        self.emit('turn', player=self.foe)
        self.emit('present_ai_dialogs', value=1)
        self.fund(0, ZERO)
        self.fund(1, ZERO)
        self.receipt()
        if self.mode in NEW_COMBAT:
            self.combat_start()
        self.cast_follower()
        if self.csvid == HOWLPACK:
            self.howlpack()
        elif self.csvid == DICTATE:
            self.dictate()
        elif self.mode == 'remove_aura':
            self.disenchant()
        elif self.mode == 'vehicle_energy':
            self.crew()
        elif self.mode == 'lethal_attack':
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.drain()
            self.emit('end_combat')
            self.receipt()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    cost, clauses, modes = MODELS[csvid]
    kind = 'Enchantment' if csvid in (HOWLPACK, DICTATE) else 'Enchantment — Aura'
    keywords = {'Flash'} if csvid in (HOWLPACK, DICTATE) else {'Flash', 'Enchant'}
    if csvid == DICTATE:
        keywords.add('Double')
    if (record.get('oracle_text') != '\n'.join(clauses) or record.get('mana_cost') != cost
            or record.get('type_line') != kind or record.get('layout') != 'normal'
            or record.get('card_faces') or record.get('selected_face') or set(record.get('keywords', ())) != keywords):
        raise ValueError('Exact whole Flash enchantment, printed cost and clauses required')
    return dict(csvid=csvid, cost=cost_from_oracle(record), clauses=list(clauses), modes=list(modes))


def variants(model):
    return [f'{mode}_{owner}' for mode in model['modes'] for owner in (0, 1)]


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(MODELS)):
        raise ValueError('Exact Flash enchantment source and witness scope required')
    for group in pins.values():
        for name, pin in group.items():
            record, row = oracle.get(name, {}), rows.get(name, {})
            layout = 'transform' if name == WEREWOLF else 'normal'
            if (not exact(pin, dict(oracle=record, registry=row)) or record.get('layout') != layout
                    or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')):
                raise ValueError(f'Changed implemented Flash source or witness: {name}')


def spec(declaration, record):
    pin = profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record, pin['oracle']):
        raise ValueError('Complete pinned Flash enchantment Oracle required')
    model = classify(record, pin['registry']['csvid'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), model) or not isinstance(selected, list) or not selected
            or any(type(v) is not str for v in selected) or len(selected) != len(set(selected))
            or set(selected) - set(variants(model))):
        raise ValueError('Exact Flash enchantment clauses, costs, and scoped variants required')
    return dict(model, family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or model['csvid'] != csvid or set(selected) - set(variants(model)):
        raise ValueError('Exact Flash enchantment primary and variants required')
    cases = []
    factory = FollowerCase if csvid in FOLLOWERS else AuraCase
    for variant in selected:
        mode, owner = variant.rsplit('_', 1)
        game = factory(oracle[name], csvid, int(owner), mode, rows, oracle)
        game.play()
        metadata = dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                        oracle_text=declaration['oracle_text'], covered_clauses=model['clauses'],
                        untested_components=[])
        cases.append(dict(name=f'FlashAurasAndEnchantmentsRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=SEED, setup=game.setup, steps=game.steps, _recipe=metadata))
    return cases
