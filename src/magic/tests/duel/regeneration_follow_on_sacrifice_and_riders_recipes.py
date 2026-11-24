"""Real sacrifice regeneration and printed combat, zone, and stat riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from recipes import cost_from_oracle

FAMILY = 'regeneration_follow_on_sacrifice_and_riders'
PROFILE = Path(__file__).with_name('data') / f'{FAMILY}_wave8_profiles.json'
LABEL = 'RegenerationFollowOnSacrificeAndRidersRecipe'
SEED = 2964
STROSSUS, GUARD, EATER, GATHERER = 4979, 7817, 13688, 8053
NIM, RHYS, WILDCATS, FIEND = 6747, 9420, 2642, 12181
COMMON = ('paid', 'denied_cost', 'proactive', 'cannot_regenerate')
MODES = {
    STROSSUS: (*COMMON, 'upkeep_cost', 'upkeep_self', 'trample'),
    GUARD: (*COMMON, 'transmute_paid', 'transmute_none', 'transmute_offturn', 'transmute_short',
            'fear_black', 'fear_artifact'),
    EATER: (*COMMON, 'destroy_paid', 'destroy_one', 'destroy_short', 'flying'),
    GATHERER: (*COMMON, 'other_attachment', 'two_attachments'),
    NIM: (*COMMON, 'own_only', 'artifact_loss'),
    RHYS: (*COMMON, 'attack_count', 'other_attacks', 'attack_self'),
    WILDCATS: (*COMMON, 'typed_forest', 'last_forest', 'forest_growth'),
    FIEND: (*COMMON, 'exile_play', 'exile_persists', 'reach_no_hit', 'empty_hand'),
}
RHYS_ATTACK_MODES = ('attack_count', 'other_attacks', 'attack_self')
FIEND_COMBAT = ('exile_play', 'exile_persists', 'reach_no_hit', 'empty_hand')
GREEN = (0, 0, 0, 1, 0, 0)
BLUE = (0, 0, 1, 0, 0, 0)
EXILED_SPELL = 'Visions of Beyond'
ZONE = dict(hand=0, battlefield=1, library=2, graveyard=3, exile=4)
ZERO = (0,) * 6
BLACK = (0, 1, 0, 0, 0, 0)
DESTROY = (2, 1, 0, 0, 0, 0)
TRANSMUTE = (1, 2, 0, 0, 0, 0)


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    pin = pins()['sources'].get(record.get('name'))
    if (csvid not in MODES or not pin or pin['registry']['csvid'] != csvid
            or not exact(record, pin['oracle']) or record.get('layout') != 'normal'):
        raise ValueError('complete normal regeneration rules required')
    return dict(csvid=csvid, cost=cost_from_oracle(record))


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['csvid']] for seat in (0, 1)]


def spec(declaration, record):
    pin = pins()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned regeneration source required')
    shape = classify(record, pin['registry']['csvid'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('whole versioned regeneration declaration required')
    if (type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(chosen) != len(set(chosen)) or set(chosen) - set(variants(shape))):
        raise ValueError('explicit distinct supported regeneration variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources', 'witnesses'):
        for name, pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None, 'NULL', '0')
                    or group == 'sources' and row['handler'] == 'card_dummy'):
                raise ValueError(f'changed regeneration fixture identity: {name}')


class Scene:
    def __init__(self, row, owner, mode, rows, oracle):
        self.name, self.id = row['name'], row['csvid']
        self.owner, self.foe, self.mode = owner, 1 - owner, mode
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.cards = [], [], {}
        self.zones = {p: {z: [] for z in ZONE} for p in (0, 1)}
        self.pools = [ZERO, ZERO]
        self.life, self.shuffles = [20, 20], [0, 0]
        self.add('source', self.name, owner, 'hand')
        if mode.startswith('transmute_'):
            self.add('library_foil', 'Forest', owner, 'library')
            if mode != 'transmute_none':
                self.add('library_hit', 'Hill Giant', owner, 'library')
            self.add('foreign_library', 'Giant Spider', self.foe, 'library')
        elif self.id == GATHERER:
            self.stage_auras()
        elif self.id == NIM:
            self.stage_nim()
        elif self.id == RHYS:
            self.stage_rhys()
        elif self.id == WILDCATS:
            self.stage_wildcats()
        elif self.id == FIEND:
            self.stage_fiend()
        elif mode.startswith('upkeep_'):
            self.add('foreign_creature', 'Walking Corpse', self.foe, 'battlefield')
            if mode == 'upkeep_cost':
                self.add('cost', 'Grizzly Bears', owner, 'battlefield')
        elif mode in ('trample', 'flying', 'fear_black', 'fear_artifact'):
            self.add('ground_foil', 'Grizzly Bears', self.foe, 'battlefield')
            bearer = {'fear_black': 'Walking Corpse', 'fear_artifact': 'Ornithopter'}.get(mode, 'Giant Spider')
            self.add('blocker', bearer, self.foe, 'battlefield')
        elif mode.startswith('destroy_'):
            self.add('cost', 'Grizzly Bears', owner, 'battlefield')
            if mode != 'destroy_one':
                self.add('second_cost', 'Walking Corpse', owner, 'battlefield')
            self.add('victim', 'Hill Giant', self.foe, 'battlefield')
            self.add('noncreature', 'Darksteel Relic', owner, 'battlefield')
        else:
            if mode == 'denied_cost':
                self.add('foreign_creature', 'Grizzly Bears', self.foe, 'battlefield')
                self.add('wrong_zone', 'Grizzly Bears', owner, 'hand')
                self.add('wrong_type', 'Darksteel Relic', owner, 'battlefield')
            else:
                self.add('cost', 'Grizzly Bears', owner, 'battlefield')
            self.stage_removal()

    def stage_removal(self):
        self.add('removal', 'Terminate' if self.mode == 'cannot_regenerate' else 'Murder', self.foe, 'hand')

    def stage_auras(self):
        owner, foe = self.owner, self.foe
        self.add('foreign_aura', 'Pacifism', foe, 'hand')
        self.add('non_aura', 'Fervor', owner, 'hand')
        if self.mode != 'denied_cost':
            self.add('cost', 'Holy Strength', owner, 'hand')
        if self.mode == 'other_attachment':
            self.add('other_host', 'Grizzly Bears', owner, 'battlefield')
        if self.mode == 'two_attachments':
            self.add('second_aura', 'Unholy Strength', owner, 'hand')
        else:
            self.stage_removal()

    def power_toughness(self):
        battlefield = self.zones[self.owner]['battlefield']
        if self.id == WILDCATS:
            forests = sum('Forest' in self.oracle[card]['type_line'].split('—')[-1].split()
                          and 'Land' in self.oracle[card]['type_line'].split('—')[0] for card in battlefield)
            size = forests + battlefield.count('Glorious Anthem')
            return size, size
        raw = self.oracle[self.name]
        power, toughness = int(raw['power']), int(raw['toughness'])
        if self.id == NIM:
            power += sum('Artifact' in self.oracle[name]['type_line'].split('—')[0] for name in battlefield)
        p, t = self.aura_bonus()
        return power + p, toughness + t

    def stage_nim(self):
        self.add('foreign_artifact', 'Darksteel Relic', self.foe, 'battlefield')
        if self.mode != 'own_only':
            self.add('own_artifact', 'Darksteel Relic', self.owner, 'battlefield')
        if self.mode == 'artifact_loss':
            self.add('lost_artifact', 'Mind Stone', self.owner, 'battlefield')
            self.add('effect', 'Naturalize', self.foe, 'hand')
        elif self.mode != 'own_only':
            if self.mode == 'denied_cost':
                self.add('wrong_controller', 'Grizzly Bears', self.foe, 'battlefield')
            else:
                self.add('cost', 'Ornithopter', self.owner, 'battlefield')
            self.stage_removal()

    def stage_rhys(self):
        self.add('foreign_elf', 'Elvish Archers', self.foe, 'battlefield')
        self.add('non_elf', 'Grizzly Bears', self.owner, 'battlefield')
        if self.mode not in ('attack_self', 'denied_cost'):
            ref = 'elf' if self.mode in RHYS_ATTACK_MODES else 'cost'
            self.add(ref, 'Elvish Warrior', self.owner, 'battlefield')
        if self.mode not in RHYS_ATTACK_MODES:
            self.stage_removal()

    def stage_wildcats(self):
        self.add('foreign_forest', 'Forest', self.foe, 'battlefield')
        self.add('nonforest', 'Island', self.owner, 'battlefield')
        if self.mode in ('denied_cost', 'typed_forest'):
            self.add('anthem', 'Glorious Anthem', self.owner, 'battlefield')
        if self.mode != 'denied_cost':
            self.add('cost', 'Taiga' if self.mode == 'typed_forest' else 'Forest', self.owner, 'battlefield')
        if self.mode in ('paid', 'proactive', 'cannot_regenerate'):
            # Both native cost paths can choose either equally identified Forest.
            self.add('remaining_forest', 'Forest', self.owner, 'battlefield')
        if self.mode == 'forest_growth':
            self.add('new_forest', 'Taiga', self.owner, 'hand')
        else:
            self.stage_removal()

    def stage_fiend(self):
        if self.mode in FIEND_COMBAT:
            self.add('ground_foil', 'Grizzly Bears', self.foe, 'battlefield')
            if self.mode == 'reach_no_hit':
                self.add('reach_foil', 'Giant Spider', self.foe, 'battlefield')
            if self.mode != 'empty_hand':
                self.add('stolen_spell', EXILED_SPELL, self.foe, 'hand')
                self.add('hand_foil', 'Mountain', self.foe, 'hand')
            if self.mode == 'exile_persists':
                self.add('removal', 'Murder', self.foe, 'hand')
            self.add('drawn', 'Island', self.owner, 'library')
            self.add('foe_library', 'Forest', self.foe, 'library')
        else:
            self.add('nonhuman', 'Grizzly Bears', self.owner, 'battlefield')
            if self.mode == 'denied_cost':
                self.add('wrong_controller', 'Expedition Envoy', self.foe, 'battlefield')
            else:
                self.add('cost', 'Expedition Envoy', self.owner, 'battlefield')
            self.stage_removal()

    def forest_growth(self):
        self.emit('play_land', 'new_forest', value=1)
        self.move('new_forest', 'battlefield')
        self.stats(tapped=0)

    def artifact_loss(self):
        self.cast('effect', 'lost_artifact')
        self.resolve_spell('effect')
        self.move('lost_artifact', 'graveyard')
        self.stats(tapped=0)

    def attack_start(self, attacker):
        self.next_turn(self.foe)
        self.next_turn(self.owner)
        self.emit('turn', player=self.owner)
        self.prop(attacker, 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', attacker, value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')

    def finish_combat(self):
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_unused_choices', value=0)

    def rhys_attack(self):
        attacker = 'non_elf' if self.mode == 'other_attacks' else 'source'
        self.attack_start(attacker)
        gained = {'attack_count': 2, 'attack_self': 1, 'other_attacks': 0}[self.mode]
        self.life[self.owner] += gained
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.emit('advance_combat')
        self.finish_combat()
        self.life[self.foe] -= 2 if attacker == 'non_elf' else 3
        self.stats(tapped=int(attacker == 'source'))

    def permission(self, actor, allowed, *, play=False):
        inputs = dict(player=actor, owner=self.foe, index=self.rows[EXILED_SPELL]['csvid'], value=int(allowed))
        self.emit('can_play_exiled', **inputs)
        if play:
            self.emit('play_exiled', **inputs)

    def fiend_attack(self):
        blocked = self.mode == 'reach_no_hit'
        exiles = self.mode in ('exile_play', 'exile_persists')
        self.attack_start('source')
        self.emit('block', 'ground_foil', other='source', value=0)
        if blocked:
            self.emit('block', 'reach_foil', other='source', value=1)
        self.emit('advance_combat')
        if exiles:
            self.emit('choose_zone_card', player=self.foe, value=self.rows[EXILED_SPELL]['csvid'])
        self.finish_combat()
        self.stats(tapped=1, damage=2 if blocked else 0)
        if not blocked:
            self.life[self.foe] -= 3
        if exiles:
            self.move('stolen_spell', 'exile')
            self.emit('assert_zone_multiplicity', player=self.foe, zone='exile',
                      index=self.rows[EXILED_SPELL]['csvid'], value=1)
        if self.mode == 'exile_persists':
            self.cast('removal', 'source')
            self.resolve_spell('removal')
            self.move('source', 'graveyard')
            self.emit('advance_turn')
            self.emit('untap_step', player=self.foe)
            self.emit('turn', player=self.foe)
        self.mana(self.foe, BLUE)
        self.permission(self.foe, False)
        self.mana(self.foe, ZERO)
        self.permission(self.owner, False)
        self.mana(self.owner, BLUE)
        self.emit('refresh')
        self.permission(self.owner, exiles, play=exiles)
        if exiles:
            # Playing the exiled spell pays and resolves it through the legacy.
            # Finish the ordinary activation left pending by that play step.
            self.emit('resolve')
            self.pools[self.owner] = ZERO
            self.move('stolen_spell', 'graveyard')
            self.move('drawn', 'hand')
            self.emit('refresh')
            self.permission(self.owner, False)
        else:
            self.mana(self.owner, ZERO)

    def aura_bonus(self):
        power = toughness = 0
        if self.id == GATHERER:
            for ref, added in (('cost', (2, 3)), ('foreign_aura', (1, 1)), ('second_aura', (3, 2))):
                card = self.cards.get(ref)
                attached = ref != 'cost' or self.mode != 'other_attachment'
                if card and card[2] == 'battlefield' and attached:
                    power += added[0]
                    toughness += added[1]
        return power, toughness

    def emit(self, op, ref='', *, prop='', **values):
        event = dict(op=op, **values)
        if ref:
            event['ref'] = ref
        if prop:
            event['property'] = prop
        self.steps.append(event)

    def add(self, ref, name, player, zone):
        assert ref not in self.cards
        self.setup.append(dict(ref=ref, name=name, player=player, zone=zone))
        self.cards[ref] = name, player, zone
        self.zones[player][zone].append(name)

    def move(self, ref, zone):
        name, player, old = self.cards[ref]
        self.zones[player][old].remove(name)
        self.zones[player][zone].append(name)
        self.cards[ref] = name, player, zone

    def prop(self, ref, field, value):
        self.emit('assert_card', ref, prop=field, value=value)

    def cast(self, ref, target=None):
        name, player, zone = self.cards[ref]
        assert zone == 'hand'
        self.mana(player, cost_from_oracle(self.oracle[name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target', target, player=player)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.prop(ref, 'in_hand', 0)
        self.pools[player] = ZERO
        self.check_mana(player)
        self.emit('assert_unused_choices', value=0)

    def resolve_spell(self, ref, destination='graveyard', *, unused=0):
        self.emit('resolve')
        self.emit('assert_unused_choices', value=unused)
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.move(ref, destination)

    def mana(self, player, amounts):
        self.pools[player] = tuple(amounts)
        for color, amount in enumerate(amounts):
            self.emit('set_mana', player=player, value=amount, color=color)

    def check_mana(self, player):
        for color, amount in enumerate(self.pools[player]):
            self.emit('assert_mana', player=player, value=amount, color=color)

    def stats(self, *, tapped, attacking=0, damage=0):
        self.prop('source', 'in_play', 1)
        self.prop('source', 'csvid', self.id)
        for field, amount in zip(('power', 'toughness'), self.power_toughness()):
            self.prop('source', field, amount)
        self.prop('source', 'tapped', tapped)
        self.prop('source', 'attacking', attacking)
        self.prop('source', 'damage', damage)

    def choose_sacrifice(self, ref):
        if self.owner == 0:
            self.emit('choose_target', ref, player=self.owner)

    def satisfy_regeneration_cost(self):
        self.move('cost', 'graveyard')
        self.pools[self.owner] = ZERO
        self.check_mana(self.owner)
        self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard',
                  index=self.rows[self.cards['cost'][0]]['csvid'], value=1)

    def preemptive(self):
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        self.choose_sacrifice('cost')
        self.emit('activate', 'source', value=1)
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=1)
        self.satisfy_regeneration_cost()
        self.emit('resolve')
        self.emit('assert_stack', value=0)
        self.emit('refresh')
        self.stats(tapped=0)
        self.cast('removal', 'source')
        self.resolve_spell('removal')
        self.stats(tapped=1)

    def destruction(self):
        paid = self.mode in ('paid', 'other_attachment', 'typed_forest', 'last_forest')
        self.cast('removal', 'source')
        self.emit('choose_regeneration', 'source')
        if paid:
            self.choose_sacrifice('cost')
        self.resolve_spell('removal', unused=int(not paid))
        if paid:
            self.satisfy_regeneration_cost()
            if self.mode == 'last_forest':
                self.move('source', 'graveyard')
            else:
                self.stats(tapped=1)
        else:
            self.move('source', 'graveyard')
            if self.id == GATHERER:
                self.move('foreign_aura', 'graveyard')
                if self.mode == 'cannot_regenerate':
                    self.move('cost', 'graveyard')

    def aura_setup(self):
        self.cast('non_aura')
        self.resolve_spell('non_aura', 'battlefield')
        if 'cost' in self.cards:
            target = 'other_host' if self.mode == 'other_attachment' else 'source'
            self.cast('cost', target)
            self.resolve_spell('cost', 'battlefield')
            self.stats(tapped=0)
        self.emit('turn', player=self.foe)
        self.cast('foreign_aura', 'source')
        self.resolve_spell('foreign_aura', 'battlefield')
        self.stats(tapped=0)
        self.emit('turn', player=self.owner)
        if self.mode == 'two_attachments':
            self.cast('second_aura', 'source')
            self.resolve_spell('second_aura', 'battlefield')
            self.stats(tapped=0)

    def next_turn(self, player):
        self.emit('advance_turn')
        self.emit('untap_step', player=player)

    def upkeep(self):
        self.next_turn(self.foe)
        self.emit('upkeep_step', player=self.foe)
        self.emit('assert_unused_choices', value=0)
        self.stats(tapped=0)
        if self.mode == 'upkeep_cost':
            self.prop('cost', 'in_play', 1)
        self.next_turn(self.owner)
        victim = 'cost' if self.mode == 'upkeep_cost' else 'source'
        self.choose_sacrifice(victim)
        self.emit('upkeep_step', player=self.owner)
        self.emit('assert_unused_choices', value=0)
        self.move(victim, 'graveyard')
        self.prop('foreign_creature', 'in_play', 1)
        if victim == 'cost':
            self.stats(tapped=0)

    def combat(self):
        self.next_turn(self.foe)
        self.next_turn(self.owner)
        self.emit('turn', player=self.owner)
        self.prop('source', 'sick', 0)
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.stats(tapped=1, attacking=1)
        self.emit('begin_blockers')
        self.emit('block', 'ground_foil', other='source', value=0)
        self.emit('block', 'blocker', other='source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_unused_choices', value=0)
        blocker = self.cards['blocker'][0]
        self.move('blocker', 'graveyard')
        self.stats(tapped=1, damage=int(self.oracle[blocker]['power']))
        if self.mode == 'trample':
            self.life[self.foe] -= 9 - int(self.oracle[blocker]['toughness'])
        self.prop('ground_foil', 'damage', 0)

    def destroy_ability(self):
        allowed = self.mode == 'destroy_paid'
        self.mana(self.owner, (1, 1, 0, 0, 0, 0) if self.mode == 'destroy_short' else DESTROY)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=int(allowed))
        if allowed:
            self.choose_sacrifice('cost')
            self.choose_sacrifice('second_cost')
            self.emit('choose_target', 'victim', player=self.owner)
        self.emit('activate', 'source', value=int(allowed))
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=int(allowed))
        if allowed:
            self.pools[self.owner] = ZERO
            self.check_mana(self.owner)
            for ref in ('cost', 'second_cost'):
                self.move(ref, 'graveyard')
                self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard',
                          index=self.rows[self.cards[ref][0]]['csvid'], value=1)
            self.prop('victim', 'in_play', 1)
            self.emit('resolve')
            self.move('victim', 'graveyard')
            self.emit('refresh')
        self.stats(tapped=0)

    def transmute(self):
        if self.mode == 'transmute_offturn':
            self.emit('turn', player=self.foe)
        budget = (1, 1, 0, 0, 0, 0) if self.mode == 'transmute_short' else TRANSMUTE
        self.mana(self.owner, budget)
        self.emit('refresh')
        permitted = self.mode in ('transmute_paid', 'transmute_none')
        self.emit('can_activate_hand', 'source', value=int(permitted))
        if permitted:
            self.emit('choose_dialog', player=self.owner, value=1)
            self.emit('activate_hand', 'source', value=1)
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_stack', value=1)
            self.pools[self.owner] = ZERO
            self.check_mana(self.owner)
            self.move('source', 'graveyard')
            self.emit('assert_zone_multiplicity', player=self.owner, zone='graveyard', index=self.id, value=1)
            self.emit('observe_search_filters', value=1)
            found = self.mode == 'transmute_paid'
            if found:
                self.emit('choose_zone_card', player=self.owner, value=self.rows['Hill Giant']['csvid'])
                self.move('library_hit', 'hand')
            else:
                self.emit('choose_zone', player=self.owner, value=-1)
            self.emit('resolve')
            self.emit('assert_unused_choices', value=0)
            self.emit('assert_offered_legal_card', index=self.rows['Forest']['csvid'], value=0)
            if found:
                self.emit('assert_offered_legal_card', index=self.rows['Hill Giant']['csvid'], value=1)
            self.shuffles[self.owner] = 1
        else:
            self.prop('source', 'in_hand', 1)

    def play(self):
        self.emit('turn', player=self.owner)
        self.emit('present_ai_dialogs', value=1)
        for player in (0, 1):
            self.mana(player, ZERO)
        if self.mode.startswith('transmute_'):
            self.transmute()
        else:
            self.cast('source')
            self.resolve_spell('source', 'battlefield')
            self.stats(tapped=0)
            if self.id == GATHERER:
                self.aura_setup()
            if self.mode.startswith('upkeep_'):
                self.upkeep()
            elif self.mode.startswith('destroy_'):
                self.destroy_ability()
            elif self.mode in ('trample', 'flying', 'fear_black', 'fear_artifact'):
                self.combat()
            elif self.mode in RHYS_ATTACK_MODES:
                self.rhys_attack()
            elif self.mode in FIEND_COMBAT:
                self.fiend_attack()
            elif self.mode == 'forest_growth':
                self.forest_growth()
            elif self.mode == 'artifact_loss':
                self.artifact_loss()
            elif self.mode not in ('two_attachments', 'own_only'):
                if self.id in (EATER, RHYS):
                    self.mana(self.owner, BLACK)
                if self.id == WILDCATS:
                    self.mana(self.owner, GREEN)
                if self.mode == 'proactive':
                    self.preemptive()
                else:
                    self.destruction()
        self.receipts()
        self.emit('present_ai_dialogs', value=0)


    def receipts(self):
        self.emit('assert_stack', value=0)
        names = sorted({name for name, _, _ in self.cards.values()})
        for player, zones in self.zones.items():
            self.check_mana(player)
            self.emit('assert_life', player=player, value=self.life[player])
            self.emit('assert_shuffle_count', player=player, value=self.shuffles[player])
            for zone, cards in zones.items():
                self.emit('assert_zone', player=player, zone=zone, value=len(cards))
                counts = Counter(cards)
                if zone == 'library':
                    for index, card in enumerate(cards):
                        self.emit('assert_zone_card', player=player, zone=zone, index=index, value=self.rows[card]['csvid'])
                for name in names:
                    self.emit('assert_zone_multiplicity', player=player, zone=zone,
                              index=self.rows[name]['csvid'], value=counts[name])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid:
        raise ValueError('regeneration source CSVID changed')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        scene = Scene(rows[name], int(seat), mode, rows, oracle)
        scene.play()
        cases.append(dict(name=f'{LABEL}_{csvid}_{variant}', card=name, fixture='empty_main', seed=SEED,
                          setup=scene.setup, steps=scene.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'])))
    return cases
