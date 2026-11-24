"""Actual Scry trigger actions and complete primary-card riders."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from mill_death_and_entry_recipes import Duel as MillBoard, stats
from recipes import cost_from_oracle


FAMILY = 'library_scry_triggers'
PROFILE = Path(__file__).with_name('data') / 'library_scry_triggers_wave8_profiles.json'
PREFIX = 'LibraryScryTriggersRecipe_'
AURA, SPHINX, REAPER, SKINK = 13376, 13520, 13533, 13974
THASSA, SHADOWS, GEIST, KINGPIN, CURATOR, SCARAB, FORESIGHT = 13572, 15215, 16107, 16327, 16879, 17238, 19751
FOLLOW_ONS = {THASSA, SHADOWS, GEIST, KINGPIN, CURATOR, SCARAB, FORESIGHT}
SCRY_ONE = 'scry 1. (Look at the top card of your library. You may put that card on the bottom.)'
PRINTED = {
    AURA: ('theros.c', '{U}', 'Enchantment — Aura', None, ('Enchant', 'Scry'),
           "Enchant creature\nEnchanted creature can't be blocked.\nWhenever enchanted creature attacks, " + SCRY_ONE),
    SPHINX: ('theros.c', '{3}{U}{U}', 'Creature — Sphinx', (3, 5), ('Scry', 'Flying'),
             'Flying\nDiscard a card: This creature gains hexproof until end of turn. Tap it.\n'
             'Whenever this creature attacks, scry 3.'),
    REAPER: ('theros.c', '{2}{B}{G}', 'Creature — Gorgon', (4, 5), ('Scry',),
             'Whenever another creature dies, scry 1.\n{B}: This creature gains deathtouch until end of turn.\n'
             '{1}{G}: This creature gains hexproof until end of turn.'),
    SKINK: ('journey_into_nyx.c', '{1}{R}', 'Creature — Lizard', (2, 1), ('Scry',),
            'Whenever this creature attacks, ' + SCRY_ONE),
    THASSA: ('theros.c', '{2}{U}', 'Legendary Enchantment Creature — God', (5, 5), ('Indestructible', 'Scry'),
             "Indestructible\nAs long as your devotion to blue is less than five, Thassa isn't a creature. "
             '(Each {U} in the mana costs of permanents you control counts toward your devotion to blue.)\n'
             "At the beginning of your upkeep, scry 1.\n{1}{U}: Target creature you control can't be blocked this turn."),
    SHADOWS: ('origins.c', '{1}{B}', 'Enchantment', None, ('Scry',),
              'Whenever a creature dies, ' + SCRY_ONE + '\n{4}{B}: Each opponent loses 2 life and you gain 2 life. '
              'Activate only if there are four or more creature cards in your graveyard.'),
    GEIST: ('eldritch_moon.c', '{2}{U}', 'Creature — Spirit', (0, 4), ('Defender', 'Scry'),
            'Defender\nAt the beginning of your upkeep, ' + SCRY_ONE),
    KINGPIN: ('kaladesh.c', '{U}{B}', 'Creature — Aetherborn Rogue', (1, 4), ('Lifelink', 'Scry'),
              'Lifelink\nWhenever an artifact you control enters, scry 1.'),
    CURATOR: ('amonkhet.c', '{2}{U}{U}', 'Creature — Sphinx', (4, 4), ('Scry', 'Flying', 'Cycling'),
              'Flying\nWhenever you cycle or discard another card, scry 1.\n'
              'Cycling {U} ({U}, Discard this card: Draw a card.)'),
    SCARAB: ('hour_of_devastation.c', '{3}{U}{B}', 'Legendary Creature — God', (5, 5), ('Scry',),
             'At the beginning of your upkeep, each opponent loses X life and you scry X, where X is the number of Zombies you control.\n'
             "{2}{U}{B}: Exile target creature card from a graveyard. Create a token that's a copy of it, except it's a 4/4 black Zombie.\n"
             "When The Scarab God dies, return it to its owner's hand at the beginning of the next end step."),
    FORESIGHT: ('eldritch_moon.c', '{2}{U}{U}', 'Creature — Sphinx', (4, 4), ('Scry', 'Flying'),
                'You may reveal this card from your opening hand. If you do, scry 3 at the beginning of your first upkeep.\n'
                'Flying\nAt the beginning of your upkeep, scry 1.'),
}
MODES = {
    AURA: ('bottom', 'keep', 'other_attack', 'foe_host', 'removed'),
    SPHINX: ('bottom', 'short', 'top_reorder', 'flying', 'discard', 'empty_hand'),
    REAPER: ('own_death', 'foe_death', 'self_death', 'noncreature', 'touch', 'hexproof'),
    SKINK: ('bottom', 'keep', 'other_attack', 'sick', 'repeat', 'empty'),
    THASSA: ('upkeep', 'devotion', 'unblockable', 'foe_only', 'indestructible'),
    SHADOWS: ('own_death', 'foe_death', 'drain', 'three', 'foe_grave'),
    GEIST: ('upkeep', 'empty_upkeep', 'defender'),
    KINGPIN: ('own_artifact', 'foe_artifact', 'nonartifact', 'lifelink'),
    CURATOR: ('self_cycle', 'other_cycle', 'own_discard', 'foe_discard', 'flying'),
    SCARAB: ('upkeep_zero', 'upkeep_two', 'copy_own', 'copy_foe', 'dies', 'exile'),
    FORESIGHT: ('upkeep', 'flying', 'opening_reveal', 'opening_pass'),
}
HOST, WALL, FLYING_WALL = 'Prodigal Sorcerer', 'Dazzling Ramparts', 'Drift of Phantasms'
KILL, BREAK, ARTIFACT, DRAW = 'Murder', 'Naturalize', 'Millstone', 'Reach Through Mists'
DISCARD = 'Plains'
AZAMI, CURSECATCHER = 'Azami, Lady of Scrolls', 'Cursecatcher'
ZOMBIE, SECOND_ZOMBIE = 'Diregraf Ghoul', 'Festering Goblin'
CYCLER, LOOTER, EXILE = 'Hieroglyphic Illumination', 'Rummaging Goblin', 'Swords to Plowshares'
ZERO = [0] * 6
STOCK = ('Island', 'Forest', 'Plains', 'Mountain', 'Swamp', 'Wastes')
OTHER_STOCK = ('Wastes', 'Swamp', 'Mountain', 'Forest', 'Plains', 'Island')
WITNESSES = set(STOCK + OTHER_STOCK) | {
    HOST, WALL, FLYING_WALL, KILL, BREAK, ARTIFACT, DRAW, AZAMI, CURSECATCHER,
    ZOMBIE, SECOND_ZOMBIE, CYCLER, LOOTER, EXILE,
}


def scenarios(csvid):
    return [(mode, actor) for mode in MODES[csvid] for actor in (0, 1)
            if not (mode == 'top_reorder' and actor == 1)]


class Duel(MillBoard):
    def __init__(self, csvid, record, actor, mode, rows, oracle):
        self.csvid, self.record, self.actor, self.foe, self.mode = csvid, record, actor, 1 - actor, mode
        self.rows, self.oracle = rows, oracle
        self.cards, self.setup, self.steps, self.life = {}, [], [], [20, 20]
        self.active = actor
        self.library = {actor: list(STOCK), self.foe: list(OTHER_STOCK)}
        self.scry_events = []
        self.add('source', actor, record['name'], 'hand')
        self.prepare_witnesses()
        for seat, cards in self.library.items():
            for index, name in enumerate(cards):
                self.add(f'library_{seat}_{index}', seat, name, 'library')

    def prepare_witnesses(self):
        mode = self.mode
        if self.csvid == AURA:
            attacker = self.foe if mode == 'foe_host' else self.actor
            self.add('host', attacker, HOST, 'battlefield')
            self.add('ground', 1 - attacker, WALL, 'battlefield')
            if mode == 'other_attack':
                self.add('unrelated', self.actor, HOST, 'battlefield')
            if mode == 'removed':
                self.add('removal', self.foe, BREAK, 'hand')
        elif self.csvid == SPHINX:
            if mode == 'short':
                self.library[self.actor] = [ARTIFACT, 'Island']
            if mode == 'flying':
                self.add('ground', self.foe, WALL, 'battlefield')
                self.add('air', self.foe, FLYING_WALL, 'battlefield')
            if mode == 'discard':
                self.add('fodder', self.actor, DISCARD, 'hand')
                self.add('own_probe', self.actor, HOST, 'battlefield')
                self.add('foe_probe', self.foe, HOST, 'battlefield')
        elif self.csvid == REAPER:
            if mode in ('own_death', 'foe_death'):
                owner = self.actor if mode == 'own_death' else self.foe
                self.add('victim', owner, HOST, 'battlefield')
            if mode == 'noncreature':
                self.add('victim', self.foe, ARTIFACT, 'battlefield')
            if mode in ('own_death', 'foe_death', 'self_death', 'noncreature'):
                self.add('removal', self.actor, BREAK if mode == 'noncreature' else KILL, 'hand')
            if mode == 'touch':
                self.add('ground', self.foe, WALL, 'battlefield')
            if mode == 'hexproof':
                self.add('own_probe', self.actor, HOST, 'battlefield')
                self.add('foe_probe', self.foe, HOST, 'battlefield')
        elif mode == 'other_attack':
            self.add('unrelated', self.actor, HOST, 'battlefield')

        if mode in ('bottom', 'foe_host', 'own_death', 'foe_death', 'touch', 'repeat'):
            self.library[self.actor][0] = ARTIFACT
        if self.csvid == SPHINX and self.actor == 1 and mode in ('bottom', 'flying'):
            first = ARTIFACT if mode == 'bottom' else 'Island'
            self.library[self.actor][:3] = [first, 'Island', 'Island']
        if mode == 'repeat':
            self.library[self.actor][1] = DRAW
        if mode == 'empty':
            self.library[self.actor] = []

    def settle_choices(self):
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_choices_consumed')

    def plan_scry(self, count, *, bottom=False, reorder=False, by_index=False):
        deck = self.library[self.actor]
        window = min(count, len(deck))
        before = deck.copy()
        self.emit('record_library', player=self.actor)
        if window:
            moved = []
            if bottom:
                chosen = deck.pop(0)
                if by_index:
                    self.emit('choose_zone', player=self.actor, value=0)
                else:
                    self.emit('choose_zone_card', player=self.actor, value=self.rows[chosen]['csvid'])
                moved.append(chosen)
            remaining = window - len(moved)
            if remaining:
                self.emit('choose_zone', player=self.actor, value=-1)
                if self.actor == 0 and remaining > 1:
                    if reorder:
                        chosen = deck.pop(remaining - 1)
                        self.emit('choose_zone_card', player=self.actor, value=self.rows[chosen]['csvid'])
                        deck.insert(0, chosen)
                    self.emit('choose_zone', player=self.actor, value=-1)
                elif self.actor == 1:
                    # Remaining copies are intentionally identical when the AI can reorder them.
                    assert len(set(deck[:remaining])) == 1
            deck.extend(moved)
        self.scry_events.append(dict(player=self.actor, count=count, before=before, after=deck.copy()))

    def check_scry(self, count):
        self.settle_choices()
        self.emit('assert_scry_result', player=self.actor, value=count)
        self.receipts()

    def age(self):
        self.go(self.foe)
        self.go(self.actor)

    def enter(self):
        self.paid('source', target='host' if self.csvid == AURA else None)
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)
        if self.csvid != AURA:
            power, toughness = stats(self.record)
            self.observe('source', 'power', power)
            self.observe('source', 'toughness', toughness)
        self.receipts()

    def attack(self, ref, *, scry=0, bottom=False, reorder=False, block=None):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        if scry:
            self.plan_scry(scry, bottom=bottom, reorder=reorder)
        self.emit('finish_attackers')
        self.emit('resolve')
        if scry:
            self.check_scry(scry)
        else:
            self.settle_choices()
            self.receipts()
        self.emit('begin_blockers')
        for blocker, legal in block or ():
            self.emit('block', blocker, ref, value=int(legal))
        self.emit('advance_combat')
        expected_blocker = next((blocker for blocker, legal in block or () if legal), None)
        self.observe(ref, 'blocked', int(expected_blocker is not None))
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power = stats(self.oracle[self.cards[ref]['name']])[0]
        if expected_blocker is None:
            self.unblocked_damage(power)
        else:
            self.observe(expected_blocker, 'damage', power)
        self.settle_choices()
        self.receipts()

    def unblocked_damage(self, power):
        self.life[1 - self.active] -= power

    def aura(self):
        if self.mode in ('removed', 'foe_host'):
            self.go(self.foe)
        if self.mode == 'removed':
            self.paid('removal', target='source')
            self.settle()
            self.cards['source']['zone'] = self.cards['removal']['zone'] = 'graveyard'
            self.go(self.actor)
        if self.mode == 'other_attack':
            self.attack('unrelated', block=[('ground', True)])
        else:
            removed = self.mode == 'removed'
            self.attack('host', scry=0 if removed else 1, bottom=self.mode in ('bottom', 'foe_host'),
                        block=[('ground', removed)])

    def targetable(self, *, protected):
        self.emit('can_target_creature', 'own_probe', 'source', value=1)
        self.emit('can_target_creature', 'foe_probe', 'source', value=int(not protected))

    def ability(self, cost, *, menu=None):
        self.mana('set_mana', self.actor, cost)
        self.emit('refresh')
        self.emit('can_activate', 'source', value=1)
        if menu is not None:
            self.emit('choose_activation_mode', 'source', player=self.actor, mode_label=menu,
                      fallback_color=0, allow_auto_nonmana=True)
        self.emit('activate', 'source', value=1)
        if menu is not None:
            self.emit('require_activation_mode', value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()

    def sphinx(self):
        if self.mode == 'empty_hand':
            self.emit('can_activate', 'source', value=0)
        elif self.mode == 'discard':
            self.age()
            self.targetable(protected=False)
            self.observe('source', 'tapped', 0)
            if self.actor == 0:
                self.emit('choose_zone_card', player=self.actor, value=self.rows[DISCARD]['csvid'])
            else:
                self.emit('discard_policy', value=1)
            self.ability(ZERO)
            self.cards['fodder']['zone'] = 'graveyard'
            self.observe('source', 'tapped', 1)
            self.targetable(protected=True)
            self.receipts()
            self.go(self.foe)
            self.targetable(protected=False)
            self.receipts()
        else:
            self.age()
            blockers = [('ground', False), ('air', True)] if self.mode == 'flying' else None
            self.attack('source', scry=3, bottom=self.mode in ('bottom', 'short'),
                        reorder=self.mode == 'top_reorder', block=blockers)

    def reaper(self):
        if self.mode == 'touch':
            self.age()
            # The actual mode has priority in the blocked-combat window.
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block', 'ground', 'source', value=1)
            self.emit('advance_combat')
            self.observe('source', 'blocked', 1)
            self.ability(cost_from_oracle({'mana_cost': '{B}'}), menu='Gain deathtouch')
            self.plan_scry(1, bottom=True)
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.cards['ground']['zone'] = 'graveyard'
            self.observe('ground', 'in_play', 0)
            self.observe('source', 'in_play', 1)
            self.check_scry(1)
        elif self.mode == 'hexproof':
            self.targetable(protected=False)
            self.ability(cost_from_oracle({'mana_cost': '{1}{G}'}), menu='Gain hexproof')
            self.targetable(protected=True)
            self.go(self.foe)
            self.targetable(protected=False)
            self.receipts()
        else:
            victim = 'source' if self.mode == 'self_death' else 'victim'
            self.paid('removal', target=victim)
            triggers = self.mode in ('own_death', 'foe_death')
            if triggers:
                self.plan_scry(1, bottom=True)
            self.settle()
            self.cards[victim]['zone'] = self.cards['removal']['zone'] = 'graveyard'
            self.observe(victim, 'in_play', 0)
            if triggers:
                self.check_scry(1)
            else:
                self.receipts()

    def skink(self):
        if self.mode == 'sick':
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.receipts()
            return
        self.age()
        if self.mode == 'other_attack':
            self.attack('unrelated')
        else:
            self.attack('source', scry=1, bottom=self.mode in ('bottom', 'repeat'))
            if self.mode == 'repeat':
                self.age()
                self.attack('source', scry=1, bottom=True)

    def build(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        self.enter()
        {AURA: self.aura, SPHINX: self.sphinx, REAPER: self.reaper, SKINK: self.skink}[self.csvid]()


class FollowOn(Duel):
    def prepare_witnesses(self):
        mode = self.mode
        self.library[self.actor][:2] = [ARTIFACT, DRAW]
        if self.csvid == THASSA:
            if mode == 'devotion':
                self.add('azami', self.actor, AZAMI, 'battlefield')
                self.add('pip', self.actor, CURSECATCHER, 'hand')
            elif mode in ('unblockable', 'foe_only'):
                self.add('foe', self.foe, HOST, 'battlefield')
                if mode == 'unblockable':
                    self.add('host', self.actor, HOST, 'battlefield')
                    self.add('ground', self.foe, WALL, 'battlefield')
            elif mode == 'indestructible':
                self.add('removal', self.foe, BREAK, 'hand')
        elif self.csvid == SHADOWS:
            if mode in ('own_death', 'foe_death'):
                seat = self.actor if mode == 'own_death' else self.foe
                self.add('victim', seat, HOST, 'battlefield')
                self.add('removal', self.actor, KILL, 'hand')
            else:
                seat = self.foe if mode == 'foe_grave' else self.actor
                for index in range(3 if mode == 'three' else 4):
                    self.add(f'grave_creature_{index}', seat, HOST, 'graveyard')
                self.add('grave_land', self.actor, 'Island', 'graveyard')
        elif self.csvid == GEIST and mode == 'empty_upkeep':
            self.library[self.actor] = []
        elif self.csvid == KINGPIN and mode != 'lifelink':
            seat = self.foe if mode == 'foe_artifact' else self.actor
            name = HOST if mode == 'nonartifact' else ARTIFACT
            self.add('entrant', seat, name, 'hand')
        elif self.csvid == CURATOR:
            if mode == 'other_cycle':
                self.add('cycler', self.actor, CYCLER, 'hand')
            if mode in ('own_discard', 'foe_discard'):
                seat = self.actor if mode == 'own_discard' else self.foe
                self.add('looter', seat, LOOTER, 'battlefield')
                self.add('fodder', seat, DISCARD, 'hand')
        elif self.csvid == SCARAB:
            if mode in ('upkeep_zero', 'upkeep_two'):
                self.add('foe_zombie', self.foe, ZOMBIE, 'battlefield')
                self.add('ordinary', self.actor, HOST, 'battlefield')
                if mode == 'upkeep_two':
                    self.add('zombie1', self.actor, ZOMBIE, 'battlefield')
                    self.add('zombie2', self.actor, SECOND_ZOMBIE, 'battlefield')
            elif mode in ('copy_own', 'copy_foe'):
                seat = self.actor if mode == 'copy_own' else self.foe
                self.add('victim', seat, HOST, 'graveyard')
                self.add('nontarget', 1 - seat, ARTIFACT, 'graveyard')
            else:
                self.add('removal', self.actor, KILL if mode == 'dies' else EXILE, 'hand')
        if mode == 'flying':
            self.add('ground', self.foe, WALL, 'battlefield')
            self.add('air', self.foe, FLYING_WALL, 'battlefield')
        if self.csvid == FORESIGHT and mode.startswith('opening_') and self.actor == 1:
            self.library[0] = ['Island', DRAW]
            self.library[1][:3] = [ARTIFACT, 'Island', 'Island']

    def unblocked_damage(self, power):
        super().unblocked_damage(power)
        if self.csvid == KINGPIN:
            self.life[self.actor] += power

    def put_source_in_play(self):
        if self.csvid not in (THASSA, SHADOWS):
            self.enter()
            return
        self.paid('source')
        self.settle()
        self.cards['source']['zone'] = 'battlefield'
        self.observe('source', 'in_play', 1)
        self.receipts()

    def upkeep(self, seat, *, scry=0, bottom=False, loss=0):
        assert seat == 1 - self.active
        self.emit('advance_turn')
        self.emit('untap_step', player=seat)
        if scry:
            assert seat == self.actor
            self.plan_scry(scry, bottom=bottom)
        self.emit('upkeep_step', player=seat)
        self.emit('turn', player=seat)
        self.active = seat
        self.life[self.foe] -= loss
        if scry:
            self.check_scry(scry)
        else:
            self.settle_choices()
            self.receipts()

    def activate_paid(self, cost, *, ref='source', target=None, grave=None):
        self.mana('set_mana', self.actor, cost_from_oracle({'mana_cost': cost}))
        self.emit('refresh')
        self.emit('can_activate', ref, value=1)
        if target is not None:
            self.emit('choose_target', target, player=self.actor)
        if grave is not None:
            self.emit('choose_zone_card', player=self.actor, value=self.rows[grave]['csvid'])
        self.emit('activate', ref, value=1)
        self.emit('assert_stack', value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.settle()

    def thassa(self):
        if self.mode in ('upkeep', 'unblockable'):
            self.upkeep(self.foe)
            self.upkeep(self.actor, scry=1, bottom=True)
            if self.mode == 'unblockable':
                self.activate_paid('{1}{U}', target='host')
                self.attack('host', block=[('ground', False)])
                self.upkeep(self.foe)
                self.upkeep(self.actor, scry=1)
                self.attack('host', block=[('ground', True)])
        elif self.mode == 'devotion':
            self.emit('can_target_creature', 'azami', 'source', value=0)
            self.paid('pip')
            self.settle()
            self.cards['pip']['zone'] = 'battlefield'
            self.emit('can_target_creature', 'azami', 'source', value=1)
            self.observe('source', 'power', 5)
            self.observe('source', 'toughness', 5)
            self.observe('source', 'in_play', 1)
            start = len(self.steps)
            self.receipts()
            # The same bound permanent has an internal creature-form ID at five devotion.
            # Its identity is observed by the reference above, alongside exact board size.
            self.steps[start:] = [action for action in self.steps[start:] if not (
                action['op'] == 'assert_zone_multiplicity' and action.get('player') == self.actor
                and action.get('zone') == 'battlefield' and action.get('index') == self.csvid)]
        elif self.mode == 'foe_only':
            pool = cost_from_oracle({'mana_cost': '{1}{U}'})
            self.mana('set_mana', self.actor, pool)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=0)
            self.mana('assert_mana', self.actor, pool)
            self.receipts()
        else:
            self.upkeep(self.foe)
            self.paid('removal', target='source')
            self.settle()
            self.cards['removal']['zone'] = 'graveyard'
            self.observe('source', 'in_play', 1)
            self.receipts()

    def shadows(self):
        if self.mode in ('own_death', 'foe_death'):
            self.paid('removal', target='victim')
            self.plan_scry(1, bottom=True)
            self.settle()
            self.cards['victim']['zone'] = self.cards['removal']['zone'] = 'graveyard'
            self.observe('victim', 'in_play', 0)
            self.check_scry(1)
        elif self.mode == 'drain':
            self.activate_paid('{4}{B}')
            self.life[self.actor] += 2
            self.life[self.foe] -= 2
            self.receipts()
        else:
            pool = cost_from_oracle({'mana_cost': '{4}{B}'})
            self.mana('set_mana', self.actor, pool)
            self.emit('refresh')
            self.emit('can_activate', 'source', value=0)
            self.mana('assert_mana', self.actor, pool)
            self.receipts()

    def geist(self):
        self.upkeep(self.foe)
        self.upkeep(self.actor, scry=1, bottom=self.mode != 'empty_upkeep')
        if self.mode == 'defender':
            self.observe('source', 'sick', 0)
            self.emit('begin_attackers')
            self.emit('attack', 'source', value=0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.settle()
            self.emit('end_combat')
            self.receipts()

    def kingpin(self):
        if self.mode == 'lifelink':
            self.age()
            self.attack('source')
            return
        if self.mode == 'foe_artifact':
            self.go(self.foe)
        self.paid('entrant')
        if self.mode == 'own_artifact':
            self.plan_scry(1, bottom=True)
        elif self.mode == 'foe_artifact':
            # Retire the legal contingency unused on compliant engines. It makes
            # incorrectly offered Scry observable without a missing-input abort.
            self.emit('choose_zone', player=self.actor, value=0)
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(self.mode == 'foe_artifact'))
        self.emit('assert_choices_consumed')
        self.emit('refresh')
        self.cards['entrant']['zone'] = 'battlefield'
        if self.mode == 'own_artifact':
            self.check_scry(1)
        else:
            self.receipts()

    def draw_one(self, seat):
        self.cards['drawn'] = dict(name=self.library[seat].pop(0), player=seat, zone='hand')

    def cycle(self, ref, *, triggers):
        self.mana('set_mana', self.actor, cost_from_oracle({'mana_cost': '{U}'}))
        self.emit('refresh')
        self.emit('require_can_activate_hand', ref, value=1)
        self.emit('choose_dialog', player=self.actor, value=1)
        if triggers:
            self.plan_scry(1, bottom=True, by_index=True)
        else:
            self.emit('choose_zone', player=self.actor, value=-1)
        self.emit('activate_hand', ref, value=1)
        self.mana('assert_mana', self.actor, ZERO)
        self.cards[ref]['zone'] = 'graveyard'
        self.emit('resolve')
        self.emit('assert_unused_choices', value=int(not triggers))
        self.emit('assert_choices_consumed')
        self.draw_one(self.actor)
        self.receipts()

    def curator(self):
        if self.mode == 'other_cycle':
            self.cycle('cycler', triggers=True)
        elif self.mode == 'flying':
            self.age()
            self.attack('source', block=[('ground', False), ('air', True)])
        else:
            if self.mode == 'own_discard':
                self.age()
            else:
                self.go(self.foe)
            self.emit('discard_policy', value=1)
            if self.mode == 'own_discard':
                self.plan_scry(1, bottom=True, by_index=True)
            self.emit('activate', 'looter', value=1)
            self.settle()
            self.cards['fodder']['zone'] = 'graveyard'
            self.observe('looter', 'tapped', 1)
            self.draw_one(self.active)
            self.receipts()

    def scarab(self):
        if self.mode.startswith('upkeep_'):
            amount = 2 if self.mode == 'upkeep_two' else 0
            self.upkeep(self.foe)
            self.upkeep(self.actor, scry=amount, bottom=bool(amount), loss=amount)
        elif self.mode.startswith('copy_'):
            self.activate_paid('{2}{U}{B}', grave=HOST)
            self.cards['victim']['zone'] = 'exile'
            self.cards['copy'] = dict(name=HOST, player=self.actor, zone='battlefield')
            for prop, value in (('token', 1), ('color', 2), ('subtype_Zombie', 1),
                                ('subtype_Human', 0), ('subtype_Wizard', 0), ('power', 4), ('toughness', 4)):
                self.emit('assert_matching_permanents', prop=prop, player=self.actor,
                          index=self.rows[HOST]['csvid'], value=value)
            self.receipts()
        else:
            self.paid('removal', target='source')
            self.settle()
            dying = self.mode == 'dies'
            self.cards['source']['zone'] = 'graveyard' if dying else 'exile'
            self.cards['removal']['zone'] = 'graveyard'
            self.life[self.actor] += 0 if dying else 5
            self.observe('source', 'in_play', 0)
            self.receipts()
            self.emit('advance_turn')
            self.active = self.foe
            if dying:
                self.cards['source']['zone'] = 'hand'
            self.receipts()

    def foresight(self):
        if self.mode == 'upkeep':
            self.upkeep(self.foe)
            self.upkeep(self.actor, scry=1, bottom=True)
        else:
            self.upkeep(self.foe)
            self.upkeep(self.actor, scry=1, bottom=True)
            self.attack('source', block=[('ground', False), ('air', True)])

    def opening(self):
        self.active = 1
        self.emit('turn', player=1)
        self.observe('source', 'in_hand', 1)
        self.receipts()
        self.emit('advance_turn')
        self.emit('untap_step', player=0)
        revealing = self.mode == 'opening_reveal'
        self.emit('choose_dialog', player=self.actor, value=int(not revealing))
        early_foreign_plan = 0
        if revealing and self.actor == 0:
            self.plan_scry(3, bottom=True, reorder=True)
        elif revealing:
            # A legacy misrouted Scry asks the human seat about its two cards.
            # Let that actual path proceed and assert no early selector or move.
            self.emit('choose_zone', player=0, value=1)
            self.emit('choose_zone', player=0, value=0)
            early_foreign_plan = 2
        self.emit('upkeep_step', player=0)
        self.emit('assert_unused_choices', value=early_foreign_plan)
        self.emit('assert_choices_consumed')
        if revealing and self.actor == 0:
            self.emit('assert_scry_result', player=0, value=3)
        self.active = 0
        self.observe('source', 'in_hand', 1)
        self.receipts()
        if self.actor == 1:
            self.upkeep(1, scry=3 if revealing else 0, bottom=revealing)
            self.observe('source', 'in_hand', 1)

    def build(self):
        self.emit('turn', player=self.actor)
        self.emit('present_ai_dialogs', value=1)
        for seat in (0, 1):
            self.mana('set_mana', seat, ZERO)
        if self.csvid == FORESIGHT and self.mode.startswith('opening_'):
            self.opening()
        elif self.csvid == CURATOR and self.mode == 'self_cycle':
            self.cycle('source', triggers=False)
        else:
            self.put_source_in_play()
            {THASSA: self.thassa, SHADOWS: self.shadows, GEIST: self.geist, KINGPIN: self.kingpin,
             CURATOR: self.curator, SCARAB: self.scarab, FORESIGHT: self.foresight}[self.csvid]()


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if (csvid not in PRINTED or not record or record.get('layout') != 'normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole normal Scry trigger record required')
    _, cost, type_line, printed_stats, keywords, rule = PRINTED[csvid]
    if (record.get('type_line') != type_line or record.get('mana_cost') != cost
            or record.get('keywords') != list(keywords) or record.get('oracle_text') != rule
            or (printed_stats is not None and stats(record) != printed_stats)):
        raise ValueError('complete printed Scry trigger and riders required')
    return dict(csvid=csvid, mana=cost_from_oracle(record), stats=list(printed_stats) if printed_stats is not None else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode, seat in scenarios(shape['csvid'])]


def require_variants(shape, selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(selected) != len(set(selected)) or set(selected) - set(variants(shape))):
        raise ValueError('distinct reviewed Scry trigger variants required')


def spec(declaration, record):
    primary = profiles()['sources'].get(record.get('name', '')) if record else None
    if not primary or not exact(primary['oracle'], record):
        raise ValueError('exact pinned Scry trigger record required')
    shape = classify(record, primary['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('complete version 2 Scry declaration required')
    require_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources', 'witnesses'} or set(pins['witnesses']) != WITNESSES
            or {pin['registry']['csvid'] for pin in pins['sources'].values()} != set(PRINTED)):
        raise ValueError('complete Scry source and witness pins required')
    if pins['witnesses'][HOST]['oracle']['type_line'] != 'Creature — Human Wizard Sorcerer':
        raise ValueError('reviewed Scarab copy witness subtypes required')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row, record = rows.get(name, {}), oracle.get(name, {})
        if (not exact(record, pin['oracle']) or not exact(row, pin['registry'])
                or row.get('kind') != 'card' or row.get('handler') in (None, '0', 'NULL', 'card_dummy')
                or record.get('layout') != 'normal' or record.get('card_faces')):
            raise ValueError('pinned Scry card changed: ' + name)
        if name in pins['sources']:
            classify(record, row['csvid'])


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    require_variants(shape, selected)
    validate_witnesses(oracle, rows)
    if profiles()['sources'][name]['registry']['csvid'] != csvid or csvid != shape['csvid']:
        raise ValueError('exact registered Scry primary required')
    cases = []
    for variant in selected:
        mode, seat = variant.rsplit('_', 1)
        implementation = FollowOn if csvid in FOLLOW_ONS else Duel
        board = implementation(csvid, oracle[name], int(seat), mode, rows, oracle)
        board.build()
        cases.append(dict(name=f'{PREFIX}{csvid}_{variant}', card=name, fixture='empty_main', seed=2857,
                          setup=board.setup, steps=board.steps, _recipe=dict(
                              family=FAMILY, version=2, variant=variant, params=declaration['params'],
                              oracle_text=declaration['oracle_text'])))
    return cases

