"""Whole-Oracle entry damage and tap scope with live payment and timing."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from recipes import card, cost_from_oracle, step

FAMILY = 'entry_trigger_damage_and_tap'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
ZONES = {'hand': 0, 'battlefield': 1, 'library': 2, 'graveyard': 3, 'exile': 4}
ZERO = (0,) * 6


class CombatBoard:
    def add(self, ref, name, player, zone, *, tapped=False):
        assert ref not in self.cards and player in (0, 1)
        self.cards[ref] = dict(name=name, player=player, zone=zone)
        flags = (1 if tapped else 8) if zone == 'battlefield' else 0
        entry = card(name, ref, player, zone)
        if flags:
            entry['state'] = ['tapped' if tapped else 'untapped']
        self.setup.append(entry)

    def emit(self, op, ref='', other='', prop='', *, player=0, value=0, zone=0, index=0, color=0):
        if op in ('untap_step', 'upkeep_step'):
            self.steps.append(step(op, player=player))
            return
        if op == 'assert_counter':
            self.steps.append(step(op, ref=ref, counter_kind=prop, value=value))
            return
        if op == 'play_land':
            self.steps.append(step(op, ref=ref, value=value))
            return
        if op == 'choose_mana_source':
            self.steps.append(step(op, ref=ref))
            return
        values = dict(ref=ref, other=other, property=prop, player=player, value=value,
                      zone=next(name for name, code in ZONES.items() if code == zone), index=index, color=color)
        self.steps.append(step(op, **{key: item for key, item in values.items() if item != ''}))

    def prop(self, ref, prop, value):
        self.emit('assert_card', ref, prop=prop, value=value)

    def fund(self, player, pool):
        self.wallet[player] = tuple(pool)
        for color, amount in enumerate(pool):
            self.emit('set_mana', player=player, color=color, value=amount)
        self.emit('refresh')

    def pools(self):
        for who, pool in enumerate(self.wallet):
            self.emit('assert_life', player=who, value=self.life[who])
            for color, amount in enumerate(pool):
                self.emit('assert_mana', player=who, color=color, value=amount)

    def move(self, ref, zone):
        self.cards[ref]['zone'] = zone

    def receipt(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)
        self.pools()
        for who in (0, 1):
            for zone, code in ZONES.items():
                names = [card['name'] for card in self.cards.values() if card['zone'] == zone and card['player'] == who]
                self.emit('assert_zone', player=who, zone=code, value=len(names))
                for name in sorted(set(names)):
                    self.emit('assert_zone_multiplicity', player=who, zone=code,
                              index=self.rows[name]['csvid'], value=names.count(name))

    def cast(self, ref, *, target=None, destination='graveyard'):
        card = self.cards[ref]
        who = card['player']
        self.fund(who, cost_from_oracle(self.oracle[card['name']]))
        if target is not None:
            self.emit('choose_target', target, player=who)
        self.emit('cast', ref, value=1)
        self.emit('assert_stack', value=1)
        self.wallet[who] = ZERO
        self.pools()
        self.emit('resolve')
        self.move(ref, destination)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=0)

    def stats(self, ref):
        r = self.oracle[self.cards[ref]['name']]
        return int(r['power']), int(r['toughness'])

    def size(self, ref, stats=None):
        self.prop(ref, 'in_play', 1)
        power, tough = self.stats(ref) if stats is None else stats
        self.prop(ref, 'power', power)
        self.prop(ref, 'toughness', tough)

    def round_trip_turn(self):
        self.emit('advance_turn')
        self.emit('advance_turn')

    def begin(self, ref, targets=None, *, cancel=False):
        self.emit('begin_attackers')
        self.emit('attack', ref, value=1)
        self.prop(ref, 'attacking', 1)
        if targets is not None:
            if self.actor == 0:
                self.emit('choose_trigger_source', ref, player=self.actor)
            for target in targets:
                self.emit('choose_target', target, player=self.actor)
            if cancel:
                self.emit('choose_cancel_target', player=self.actor)
            self.targeted.extend(targets)
        self.emit('finish_attackers')
        self.emit('resolve')
        self.emit('assert_unused_choices', value=0)
        self.emit('assert_stack', value=0)

    def combat(self, attacker, *, attempts=(), blocked=None, actual_stats=None, after_advance=None):
        self.emit('begin_blockers')
        for blocker, allowed in attempts:
            self.emit('block', blocker, attacker, value=int(allowed))
        self.emit('advance_combat')
        self.prop(attacker, 'blocked', int(blocked is not None))
        if after_advance:
            after_advance()
        if blocked is not None:
            self.prop(blocked, 'blocking_count', 1)
        a_pt = self.stats(attacker)
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if blocked is None:
            self.life[self.foe] -= a_pt[0]
        else:
            b_pt = actual_stats or self.stats(blocked)
            for ref, received, toughness in ((attacker, b_pt[0], a_pt[1]), (blocked, a_pt[0], b_pt[1])):
                if received >= toughness:
                    self.move(ref, 'graveyard')
                else:
                    self.prop(ref, 'damage', received)
        self.receipt()

MARE, SQUID, SLINGER, ANGEL = 3053, 4836, 4000, 16205
PLAN = {
    MARE: ('{5}{R}', 'Creature — Elemental Horse', '5', '5',
           'Haste (This creature can attack and {T} as soon as it comes under your control.)\n'
           'When this creature enters, tap all other creatures.',
           ('scope','haste','no_other','countered')),
    SQUID: ('{3}{U}', 'Creature — Squid Beast', '2', '2',
            'When this creature enters, tap all lands target player controls.',
            ('foe','own','empty','artifact_land','countered')),
    SLINGER: ('{2}{R}', 'Creature — Human Nomad', '2', '2',
              'Echo {2}{R} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)\n'
              'When this creature enters, it deals 2 damage to any target.',
              ('kill','mark','player','planeswalker','self','echo_pay','echo_decline','echo_short','countered')),
    ANGEL: ('{4}{W}{W}', 'Creature — Angel', '4', '3',
            'Flying\nWhen this creature enters, tap all creatures your opponents control.',
            ('scope','ground_block','flying_block','countered')),
}
WITNESSES = {
    'Goblin Tinkerer': ('{1}{R}', '{R}, {T}: Destroy target artifact. That artifact deals damage equal to its mana value to this creature.', '1','2'),
    'Prodigal Sorcerer': ('{2}{U}', '{T}: This creature deals 1 damage to any target.', '1','1'),
    'Serra Angel': ('{3}{W}{W}', "Flying\nVigilance (Attacking doesn't cause this creature to tap.)", '4','4'),
    'Island': ('','({T}: Add {U}.)'),
    'Forest': ('','({T}: Add {G}.)'),
    'Mountain': ('','({T}: Add {R}.)'),
    'Wastes': ('','{T}: Add {C}.'),
    'Seat of the Synod': ('','{T}: Add {U}.'),
    'Welding Jar': ('{0}','Sacrifice this artifact: Regenerate target artifact.'),
    'Essence Scatter': ('{1}{U}','Counter target creature spell.'),
}
SEED = 3152


def label(csvid, mode, actor):
    return f'EntryTriggerDamageAndTapRecipe_{csvid}_{mode}_{actor}'


class Board(CombatBoard):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid, self.mode, self.actor, self.foe = csvid,mode,actor,1-actor
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards = [],[],{}
        self.wallet,self.life = [ZERO,ZERO],[20,20]
        self.add('source',record['name'],actor,'hand')
        self.entry_target, self.echo_lands = None, []
        if csvid in (MARE,SQUID,ANGEL):
            if mode != 'no_other':
                self.add('ally','Goblin Tinkerer',actor,'battlefield')
                self.add('guard','Serra Angel' if mode == 'flying_block' else 'Goblin Tinkerer',self.foe,'battlefield')
            self.add('own_land','Island',actor,'battlefield')
            if mode != 'empty':
                self.add('foe_land','Seat of the Synod' if mode == 'artifact_land' else 'Forest',self.foe,'battlefield')
                if csvid == SQUID:
                    self.add('foe_land_2','Island',self.foe,'battlefield')
            self.add('own_artifact','Welding Jar',actor,'battlefield')
            self.add('foe_artifact','Welding Jar',self.foe,'battlefield')
        if mode == 'countered':
            self.add('counter','Essence Scatter',self.foe,'hand')
        if csvid == SLINGER:
            if mode in ('kill','mark'):
                self.add('guard','Prodigal Sorcerer' if mode == 'kill' else 'Serra Angel',self.foe,'battlefield')
            elif mode == 'planeswalker':
                self.add('walker','Jace Beleren',self.foe,'hand')
            elif mode in ('echo_pay','echo_decline','echo_short'):
                for n, land in enumerate(self.echo_sources()):
                    ref = f'echo_{n}'
                    self.add(ref,land,actor,'battlefield')
                    self.echo_lands.append(ref)

    def echo_sources(self):
        if self.mode == 'echo_short':
            return ('Wastes','Wastes')
        return ('Wastes','Wastes','Mountain') if self.actor == 0 else ('Mountain',)*3

    def pending(self, ref, *, response=False):
        card = self.cards[ref]
        who = card['player']
        self.fund(who,cost_from_oracle(self.oracle[card['name']]))
        self.emit('cast_response' if response else 'cast',ref,value=1)
        self.emit('assert_stack',value=2 if response else 1)
        self.wallet[who] = ZERO
        self.pools()

    def cast_entry(self):
        self.pending('source')
        if self.mode == 'countered':
            self.emit('announce_spell','source')
            self.pending('counter',response=True)
            self.emit('resolve_response')
            self.move('source','graveyard')
            self.move('counter','graveyard')
            return
        if self.csvid == SQUID:
            self.entry_target = self.actor if self.mode == 'own' else self.foe
            self.emit('choose_player',player=self.actor,value=self.entry_target)
        if self.csvid == SLINGER:
            self.entry_target = {'kill':'guard','mark':'guard','planeswalker':'walker','self':'source'}.get(self.mode,self.foe)
            if isinstance(self.entry_target,int):
                self.emit('choose_player',player=self.actor,value=self.entry_target)
            else:
                self.emit('choose_target',self.entry_target,player=self.actor)
        self.emit('resolve')
        self.move('source','battlefield')
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=0)

    def name_is(self,ref,kind):
        return kind in self.oracle[self.cards[ref]['name']]['type_line'].split(' — ')[0].split(' ')

    def tap_receipt(self):
        for ref, card in self.cards.items():
            if card['zone'] != 'battlefield':
                continue
            tap = False
            if self.mode != 'countered' and ref != 'source':
                if self.csvid == MARE:
                    tap = self.name_is(ref,'Creature')
                elif self.csvid == ANGEL:
                    tap = card['player'] == self.foe and self.name_is(ref,'Creature')
                elif self.csvid == SQUID:
                    tap = card['player'] == self.entry_target and self.name_is(ref,'Land')
            self.prop(ref,'tapped',int(tap))

    def fight(self):
        if self.csvid == MARE:
            self.prop('source','sick',0)
            self.begin('source')
            self.combat('source',attempts=(('guard',False),))
            return
        self.prop('source','sick',1)
        self.emit('advance_turn')
        self.emit('untap_step',player=self.foe)
        self.prop('guard','tapped',0)
        self.emit('advance_turn')
        self.prop('source','sick',0)
        self.begin('source')
        allowed = self.mode == 'flying_block'
        self.combat('source',attempts=(('guard',allowed),),blocked='guard' if allowed else None)

    def echo(self):
        self.emit('advance_turn')
        self.emit('untap_step',player=self.foe)
        self.emit('upkeep_step',player=self.foe)
        self.prop('source','in_play',1)
        self.receipt()
        self.emit('advance_turn')
        self.emit('untap_step',player=self.actor)
        paid = self.mode == 'echo_pay'
        expect_offer = self.mode != 'echo_short'
        if expect_offer:
            self.emit('choose_dialog',player=self.actor,value=int(not paid))
        if paid and self.actor == 0:
            for ref in self.echo_lands:
                self.emit('choose_mana_source',ref)
        self.emit('upkeep_step',player=self.actor)
        self.emit('assert_unused_choices',value=0)
        for ref in self.echo_lands:
            self.prop(ref,'tapped',int(paid))
        if not paid:
            self.move('source','graveyard')
        else:
            self.prop('source','in_play',1)
        self.receipt()
        if paid:
            self.emit('advance_turn')
            self.emit('advance_turn')
            self.emit('untap_step',player=self.actor)
            self.emit('upkeep_step',player=self.actor)
            self.prop('source','in_play',1)
            for ref in self.echo_lands:
                self.prop(ref,'tapped',0)

    def damage_receipt(self):
        if self.mode == 'kill':
            self.move('guard','graveyard')
        elif self.mode == 'mark':
            self.size('guard')
            self.prop('guard','damage',2)
        elif self.mode == 'planeswalker':
            self.emit('assert_counter','walker',prop='loyalty',value=1)
        elif self.mode == 'self':
            self.move('source','graveyard')
        else:
            self.life[self.foe] -= 2
        if self.mode != 'self':
            self.size('source')
        self.receipt()
        if self.echo_lands:
            self.echo()

    def play(self):
        self.emit('turn',player=self.foe if self.mode == 'planeswalker' else self.actor)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,ZERO)
        if self.mode == 'planeswalker':
            self.cast('walker',destination='battlefield')
            self.emit('assert_counter','walker',prop='loyalty',value=3)
            self.emit('advance_turn')
        self.cast_entry()
        if self.mode != 'countered' and self.csvid == SLINGER:
            self.damage_receipt()
        elif self.csvid != SLINGER:
            self.tap_receipt()
            if self.mode != 'countered':
                self.size('source')
            self.receipt()
            if self.mode in ('haste','ground_block','flying_block'):
                self.fight()
        self.receipt()


MOGG, TIMBER, SWORD, MAGMA, CRATER, DRAGON = 4760, 8864, 17490, 3656, 3734, 4375
RIG, DESERT, FANATIC, CHAMPION, DIADEM, GIANT = 11963, 17036, 13437, 4148, 13928, 13779
FOLLOWERS = {
    4760: ('{1}{R}', 'Creature — Goblin', '1', '1', 'Haste\nWhen this creature enters, tap all other creatures.', ('Haste',), ('scope', 'haste', 'countered')),
    8864: ('{3}{G}', 'Creature — Elemental Horse', '5', '5', 'Haste\nEcho {5}{G} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)\nWhen this creature enters, tap all other creatures.', ('Haste', 'Echo'), ('scope', 'haste', 'echo_pay', 'echo_short')),
    17490: ('{3}{R}{G}', 'Creature — Dinosaur', '5', '5', 'Trample\nWhen this creature enters, it deals 1 damage to each other creature.', ('Trample',), ('scope', 'trample', 'countered')),
    3656: ('{5}{R}{R}', 'Creature — Giant', '5', '5', 'When this creature enters, it deals 2 damage to each creature and each player.', (), ('scope', 'both_players', 'countered')),
    3734: ('{4}{R}{R}', 'Creature — Hellion Beast', '6', '6', 'Echo {4}{R}{R} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)\nWhen this creature enters, it deals 4 damage to each other creature.', ('Echo',), ('scope', 'echo_pay', 'echo_decline', 'countered')),
    4375: ('{5}{R}{R}', 'Creature — Dragon', '5', '5', 'Flying\nWhen this creature enters, it deals 3 damage to each creature without flying.', ('Flying',), ('scope', 'ground_block', 'flying_block')),
    11963: ('{6}', 'Artifact Creature — Construct', '4', '5', 'When this creature enters, you may have it deal 4 damage to target creature with flying.', (), ('kill_flyer', 'decline', 'no_flyer')),
    17036: ('', 'Land — Desert', None, None, 'When this land enters, it deals 1 damage to target player or planeswalker.\n{T}: Add {C}.', (), ('opponent', 'self', 'planeswalker', 'mana')),
    13437: ('{3}{R}', 'Creature — Minotaur Shaman', '4', '2', 'When this creature enters, it deals damage to each opponent equal to your devotion to red. (Each {R} in the mana costs of permanents you control counts toward your devotion to red.)', (), ('self_only', 'own_symbols', 'hybrid', 'support_lost')),
    4148: ('{2}{R}{R}', 'Creature — Human Barbarian', '3', '2', 'Haste\nEcho {2}{R}{R} (At the beginning of your upkeep, if this came under your control since the beginning of your last upkeep, sacrifice it unless you pay its echo cost.)\nWhen this creature enters, it deals 3 damage to target player or planeswalker.', ('Haste', 'Echo'), ('player', 'planeswalker', 'haste', 'echo_pay', 'echo_decline', 'echo_short')),
    13928: ('{5}{R}', 'Enchantment — Aura', None, None, 'Enchant creature\nWhen this Aura enters, it deals 2 damage to any target.\nEnchanted creature gets +2/+2.', ('Enchant',), ('player', 'lethal', 'detach', 'countered')),
    13779: ('{4}{R}', 'Creature — Giant', '3', '3', "Tribute 2 (As this creature enters, an opponent of your choice may put two +1/+1 counters on it.)\nWhen this creature enters, if tribute wasn't paid, this creature deals 5 damage to each opponent.", ('Tribute',), ('tribute_paid', 'tribute_declined', 'countered')),
}
NEW_WITNESSES = {
    'Terra Stomper': ('{3}{G}{G}{G}', "This spell can't be countered.\nTrample (This creature can deal excess combat damage to the player or planeswalker it's attacking.)", '8','8'),
    'Fiery Hellhound': ('{1}{R}{R}', '{R}: This creature gets +1/+0 until end of turn.', '2','2'),
    'Boros Reckoner': ('{R/W}{R/W}{R/W}', 'Whenever this creature is dealt damage, it deals that much damage to any target.\n{R/W}: This creature gains first strike until end of turn.', '3','3'),
    'Unsummon': ('{U}', "Return target creature to its owner's hand."),
    'Counterspell': ('{U}{U}', 'Counter target spell.'),
}
PILOT_KEYWORDS = {MARE: ('Haste',), SLINGER: ('Echo',), ANGEL: ('Flying',)}
AREA = {SWORD:(1,False,False), MAGMA:(2,True,False), CRATER:(4,False,False), DRAGON:(3,False,True)}
ECHO_COSTS = {TIMBER:(5,1,'Forest'), CRATER:(4,2,'Mountain'), CHAMPION:(2,2,'Mountain')}


class FollowOn(Board):
    def __init__(self, record, csvid, mode, actor, rows, oracle):
        self.csvid,self.mode,self.actor,self.foe=csvid,mode,actor,1-actor
        self.rows,self.oracle=rows,oracle
        self.setup,self.steps,self.cards=[],[],{}
        self.wallet,self.life=[ZERO,ZERO],[20,20]
        self.entry_target,self.echo_lands=None,[]
        self.add('source',record['name'],actor,'hand')
        if mode == 'countered':
            self.add('counter','Counterspell' if csvid==DIADEM else 'Essence Scatter',self.foe,'hand')
        if csvid in (MOGG,TIMBER):
            self.add('ally','Goblin Tinkerer',actor,'battlefield')
            self.add('guard','Goblin Tinkerer',self.foe,'battlefield')
            self.add('land','Island',self.foe,'battlefield')
            self.add('relic','Welding Jar',actor,'battlefield')
        if csvid in AREA:
            self.area_setup()
        if csvid == RIG:
            self.add('ground','Goblin Tinkerer',actor,'battlefield')
            self.add('guard','Goblin Tinkerer' if mode=='no_flyer' else 'Serra Angel',self.foe,'battlefield')
        if csvid == FANATIC:
            if mode != 'self_only':
                self.add('opposing_red','Fiery Hellhound',self.foe,'battlefield')
                self.add('spark','Boros Reckoner' if mode=='hybrid' else 'Fiery Hellhound',actor,'battlefield')
                self.add('relic','Welding Jar',actor,'battlefield')
            if mode == 'support_lost':
                self.add('bounce','Unsummon',self.foe,'hand')
        if csvid == DIADEM:
            self.add('host','Fiery Hellhound',actor,'battlefield')
            self.add('guard','Goblin Tinkerer',self.foe,'battlefield')
            if mode == 'detach':
                self.add('bounce','Unsummon',self.foe,'hand')
        if mode == 'planeswalker':
            self.add('walker','Jace Beleren',self.foe,'hand')
        if mode.startswith('echo_'):
            for n,name in enumerate(self.echo_sources()):
                ref=f'echo_{n}'
                self.add(ref,name,actor,'battlefield')
                self.echo_lands.append(ref)

    def area_setup(self):
        if (self.csvid, self.mode, self.actor) != (CRATER, 'echo_pay', 1):
            self.add('land','Forest',self.actor,'battlefield')
        self.add('relic','Welding Jar',self.foe,'battlefield')
        base = 'Prodigal Sorcerer' if self.csvid == SWORD else 'Goblin Tinkerer'
        if self.csvid == MAGMA and self.mode == 'both_players':
            return
        self.add('ally',base,self.actor,'battlefield')
        self.add('guard','Goblin Tinkerer' if self.csvid==SWORD else 'Terra Stomper',self.foe,'battlefield')
        self.add('flyer','Serra Angel',self.foe,'battlefield')

    def echo_sources(self):
        generic,count,color=ECHO_COSTS[self.csvid]
        if self.mode == 'echo_short':
            return ('Wastes',)*generic
        return ('Wastes',)*generic+(color,)*count if self.actor==0 else (color,)*(generic+count)

    def begin_sourced_spell(self):
        if self.csvid==DIADEM:
            # Aura spell target is locked before ETB has picked its damage target.
            fee=cost_from_oracle(self.oracle['Lightning Diadem'])
            self.fund(self.actor,fee)
            self.emit('choose_target','host',player=self.actor)
            self.emit('cast','source',value=1)
            self.emit('assert_stack',value=1)
            self.wallet[self.actor]=ZERO
            self.pools()
        else:
            self.pending('source')

    def begin_resolve(self):
        self.begin_sourced_spell()
        if self.mode == 'countered':
            self.emit('announce_spell','source')
            self.pending('counter',response=True)
            self.emit('resolve_response')
            self.move('source','graveyard')
            self.move('counter','graveyard')
            return False
        if self.mode == 'support_lost':
            self.emit('announce_spell','source')
            name=self.cards['bounce']['name']
            self.fund(self.foe,cost_from_oracle(self.oracle[name]))
            self.emit('choose_target','spark',player=self.foe)
            self.emit('cast_response','bounce',value=1)
            self.emit('assert_stack',value=2)
            self.wallet[self.foe]=ZERO
            self.pools()
            self.emit('resolve_response')
            self.move('bounce','graveyard')
            self.move('spark','hand')
        else:
            self.choice_inputs()
            self.emit('resolve')
        self.move('source','battlefield')
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=0)
        return True

    def choice_inputs(self):
        if self.csvid == CHAMPION:
            self.emit('choose_target','walker',player=self.actor) if self.mode=='planeswalker' else self.emit('choose_player',player=self.actor,value=self.foe)
        if self.csvid == DIADEM:
            self.emit('choose_target','guard',player=self.actor) if self.mode=='lethal' else self.emit('choose_player',player=self.actor,value=self.foe)
        if self.csvid == GIANT:
            self.emit('choose_dialog',player=self.foe,value=int(self.mode!='tribute_paid'))
        if self.csvid == RIG:
            if self.mode == 'kill_flyer':
                if self.actor==0:
                    self.emit('choose_priority','source')
                self.emit('choose_target','guard',player=self.actor)
            elif self.mode == 'decline' and self.actor==1:
                self.emit('choose_cancel_target',player=1)

    def tap_others(self):
        self.size('source')
        for ref,value in (('source',0),('ally',1),('guard',1),('land',0),('relic',0)):
            self.prop(ref,'tapped',value)
        self.receipt()
        if self.mode == 'haste':
            self.prop('source','sick',0)
            self.begin('source')
            self.combat('source',attempts=(('guard',False),))

    def area_damage(self):
        amount, hits_source, only_ground = AREA[self.csvid]
        if hits_source:
            self.life[self.actor]-=amount
            self.life[self.foe]-=amount
        for ref,card in self.cards.items():
            if card['zone']!='battlefield':
                continue
            if not self.name_is(ref,'Creature'):
                self.prop(ref,'tapped',0)
                continue
            if ref=='source':
                damage=amount if hits_source else 0
            elif only_ground and 'Flying' in self.oracle[card['name']]['keywords']:
                damage=0
            else:
                damage=amount
            if damage>=self.stats(ref)[1]:
                self.move(ref,'graveyard')
            else:
                self.size(ref)
                self.prop(ref,'damage',damage)
        self.receipt()
        if self.mode in ('ground_block','flying_block'):
            self.fight_with_dragon()
        if self.mode == 'trample':
            self.round_trip_turn()
            self.begin('source')
            self.emit('begin_blockers')
            self.emit('block','guard','source',value=1)
            self.emit('advance_combat')
            self.prop('source','blocked',1)
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.foe]-=3
            self.prop('source','damage',1)
            self.move('guard','graveyard')

    def fight_with_dragon(self):
        flying=self.mode=='flying_block'
        self.round_trip_turn()
        self.begin('source')
        blocker='flyer' if flying else 'guard'
        self.combat('source',attempts=((blocker,flying),),blocked=blocker if flying else None)

    def sunscorched(self):
        if self.mode == 'planeswalker':
            self.emit('choose_target','walker',player=self.actor)
        else:
            target = self.actor if self.mode=='self' else self.foe
            self.emit('choose_player',player=self.actor,value=target)
        self.emit('play_land','source',value=1)
        self.move('source','battlefield')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        if self.mode == 'planeswalker':
            self.emit('assert_counter','walker',prop='loyalty',value=2)
        else:
            self.life[target]-=1
        self.prop('source','tapped',0)
        self.receipt()
        if self.mode == 'mana':
            self.emit('can_activate','source',value=1)
            self.emit('activate','source',value=1)
            self.wallet[self.actor]=(1,0,0,0,0,0)
            self.prop('source','tapped',1)
            self.emit('can_activate','source',value=0)

    def fanatic(self):
        damage=0
        for card in self.cards.values():
            if card['player']==self.actor and card['zone']=='battlefield':
                symbols=self.oracle[card['name']]['mana_cost'].split('}')
                damage+=sum('R' in symbol[1:].split('/') for symbol in symbols if symbol.startswith('{'))
        self.life[self.foe]-=damage
        self.size('source')

    def champion(self):
        if self.mode == 'planeswalker':
            self.emit('refresh')
            self.move('walker','graveyard')
        else:
            self.life[self.foe]-=3
        self.size('source')
        self.prop('source','sick',0)
        self.receipt()
        if self.mode == 'haste':
            self.begin('source')
            self.combat('source')

    def diadem(self):
        self.size('host',(4,4))
        if self.mode=='lethal':
            self.move('guard','graveyard')
        else:
            self.life[self.foe]-=2
        self.receipt()
        if self.mode=='detach':
            self.cast('bounce',target='host')
            self.move('host','hand')
            self.move('source','graveyard')

    def giant(self):
        paid = self.mode=='tribute_paid'
        self.emit('assert_counter','source',prop='plus1',value=2 if paid else 0)
        self.size('source',(5,5) if paid else (3,3))
        if not paid:
            self.life[self.foe]-=5

    def rig(self):
        self.size('source')
        self.size('ground')
        self.prop('ground','damage',0)
        if self.mode == 'kill_flyer':
            self.move('guard','graveyard')
        else:
            self.size('guard')
            self.prop('guard','damage',0)

    def play(self):
        self.emit('turn',player=self.foe if self.mode=='planeswalker' else self.actor)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,ZERO)
        if self.mode=='planeswalker':
            self.cast('walker',destination='battlefield')
            self.emit('assert_counter','walker',prop='loyalty',value=3)
            self.emit('advance_turn')
        if self.csvid==DESERT:
            self.sunscorched()
        elif self.begin_resolve():
            actions={MOGG:self.tap_others,TIMBER:self.tap_others,RIG:self.rig,FANATIC:self.fanatic,
                     CHAMPION:self.champion,DIADEM:self.diadem,GIANT:self.giant}
            actions.get(self.csvid,self.area_damage)()
            if self.echo_lands:
                self.echo()
        else:
            for ref,card in self.cards.items():
                if card['zone']=='battlefield':
                    self.prop(ref,'tapped',0)
        self.receipt()


@lru_cache
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid in PLAN:
        cost,kind,power,tough,rules,modes=PLAN[csvid]
        keywords=PILOT_KEYWORDS.get(csvid,())
    else:
        cost,kind,power,tough,rules,keywords,modes=FOLLOWERS[csvid]
    fields=('mana_cost','type_line','power','toughness','oracle_text')
    if tuple(record.get(field) for field in fields)!=(cost,kind,power,tough,rules):
        raise ValueError('Exact cost, creature stats and all printed entry clauses required')
    if record.get('layout')!='normal' or record.get('card_faces') or set(record.get('keywords',()))!=set(keywords):
        raise ValueError('Exact normal single-face entry record required')
    return dict(csvid=csvid,cost=cost,choices=[f'{mode}_{seat}' for mode in modes for seat in (0,1)])


def variants(model):
    return list(model['choices'])


def validate_witnesses(oracle, rows):
    pins=profiles()
    expects=WITNESSES | NEW_WITNESSES
    if (set(pins)!={'sources','witnesses'} or set(pins['witnesses'])!=expects.keys()|{'Jace Beleren'}
            or {pin['registry']['csvid'] for pin in pins['sources'].values()}!=PLAN.keys()|FOLLOWERS.keys()):
        raise ValueError('Exact source and witness scope required')
    for group in pins.values():
        for name,pin in group.items():
            record,row=oracle.get(name,{}),rows.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout')!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy')):
                raise ValueError(f'Changed implemented entry source/witness: {name}')
            if name in expects:
                fields=('mana_cost','oracle_text','power','toughness')[:len(expects[name])]
                if tuple(record[k] for k in fields)!=expects[name]:
                    raise ValueError(f'Changed witness Oracle clauses: {name}')


def spec(declaration, record):
    pin=profiles()['sources'].get((record or {}).get('name'))
    if not pin or not exact(record,pin['oracle']):
        raise ValueError('Complete pinned primary required')
    model=classify(record,pin['registry']['csvid'])
    chosen=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),model) or not isinstance(chosen,list) or not chosen
            or any(type(v) is not str for v in chosen) or len(chosen)!=len(set(chosen)) or set(chosen)-set(variants(model))):
        raise ValueError('Exact family and explicit distinct entry variants required')
    return dict(model,family=FAMILY)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or model['csvid']!=csvid or set(selected)-set(variants(model)):
        raise ValueError('Exact primary and scoped entry choices required')
    factory=FollowOn if csvid in FOLLOWERS else Board
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        board=factory(oracle[name],csvid,mode,int(seat),rows,oracle)
        board.play()
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                      oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                      untested_components=[])
        cases.append(dict(name=label(csvid,mode,int(seat)),card=name,fixture='empty_main',seed=SEED,
                          setup=board.setup,steps=board.steps,_recipe=metadata))
    return cases
