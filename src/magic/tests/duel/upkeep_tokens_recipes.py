"""Oracle-pinned upkeep tokens, copies, and printed combat or activation riders."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from generate_cases import OPS
from recipes import cost_from_oracle

PROFILE = Path(__file__).with_name("data") / "upkeep_tokens_wave8_profiles.json"

LEGION, ZONE, BLOSSOM, BRINGER = 12885, 10873, 1440, 7022
PREFIX, SEED = "UpkeepTokensRecipe", 3686
FAMILY = "upkeep_tokens"
MODES = {
    LEGION: ("ramp", "haste", "reset", "removed"),
    ZONE: ("accept", "decline", "spawn_mana", "removed"),
    BLOSSOM: ("repeat", "flying", "kindred", "removed"),
    BRINGER: ("normal", "rainbow", "decline", "trample", "missing_blue"),
}
HUMAN_ONLY = {(ZONE, "decline"), (BRINGER, "decline")}
CONTROLS = (
    (LEGION, "ramp", 0), (LEGION, "haste", 1),
    (ZONE, "decline", 0), (ZONE, "spawn_mana", 1),
    (BLOSSOM, "flying", 0), (BLOSSOM, "repeat", 1),
    (BRINGER, "rainbow", 0), (BRINGER, "trample", 1),
)

SOLDIER, SPAWN, FAERIE, BEAST = "Soldier", "Eldrazi Spawn", "Faerie Rogue", "Beast"
TOKEN = {
    SOLDIER: (18822, 48, 1, 1, ("Soldier",)),
    SPAWN: (18725, 0, 0, 1, ("Eldrazi", "Spawn")),
    FAERIE: (18736, 2, 1, 1, ("Rogue",)),
    BEAST: (18695, 8, 3, 3, ("Beast",)),
}
SNAP, DISENCHANT, BONES = "Aether Snap", "Disenchant", "Bonesplitter"
WALL, BEAR, PEPPER, FOREST = "Wall of Stone", "Grizzly Bears", "Peppersmoke", "Forest"
WITNESSES = {SNAP, DISENCHANT, BONES, WALL, BEAR, PEPPER, FOREST, *TOKEN}
HAND, BATTLEFIELD, LIBRARY, GRAVEYARD, EXILE = range(5)
ZERO = (0,) * 6
RAINBOW = (0, 1, 1, 1, 1, 1)


def label(csvid, mode, owner):
    return f"{PREFIX}_{csvid}_{mode}_{owner}"


class Board:
    def __init__(self, row, mode, owner, rows, oracle):
        self.id, self.name = row["csvid"], row["name"]
        self.mode, self.owner, self.foe = mode, owner, 1 - owner
        self.rows, self.oracle = rows, oracle
        self.setup, self.steps, self.objects = [], [], {}
        self.life = [20, 20]
        self.active = owner
        self.tokens = Counter()
        self.token = {LEGION: SOLDIER, ZONE: SPAWN, BLOSSOM: FAERIE, BRINGER: BEAST}[self.id]
        self.add("source", self.name, owner, HAND)
        if mode == "removed":
            self.add("remove", DISENCHANT, self.foe, HAND)
        if mode == "reset":
            self.add("snap", SNAP, self.owner, HAND)
        if mode == "spawn_mana":
            self.add("receiver", BONES, self.owner, HAND)
        if mode == "flying":
            self.add("blocker", WALL, self.foe, BATTLEFIELD)
        if mode in ("trample", "kindred"):
            self.add("blocker", BEAR, self.foe, BATTLEFIELD)
        if mode == "kindred":
            self.add("pepper", PEPPER, self.owner, HAND)
            self.add("top", FOREST, self.owner, LIBRARY)

    def add(self, ref, name, player, zone):
        assert ref not in self.objects
        self.objects[ref] = [name, player, zone]
        self.setup.append((ref, player, zone, self.rows[name]["csvid"], 0))

    def emit(self, op, ref="", other="", prop="", *, player=0, value=0, zone=0, index=0, color=0):
        assert op in OPS
        self.steps.append((op, ref, other, prop, player, value, zone, index, color))

    def mana(self, op, seat, values=ZERO):
        for color, value in enumerate(values):
            self.emit(op, player=seat, color=color, value=value)

    def fund(self, seat, values=ZERO):
        self.mana("set_mana", seat, values)
        self.emit("refresh")

    def observe(self, ref, prop, value):
        self.emit("assert_card", ref, prop=prop, value=value)

    def resolve(self):
        self.emit("resolve")
        self.emit("refresh")
        self.emit("assert_unused_choices")
        self.emit("assert_stack")

    def cast(self, ref, *, target=None, fee=None, already_funded=False, allowed=True):
        name, player, zone = self.objects[ref]
        assert zone == HAND
        fee = cost_from_oracle(self.oracle[name]) if fee is None else fee
        if not already_funded:
            self.fund(player, fee)
        if target:
            self.emit("choose_target", target, player=player)
        self.emit("cast", ref, value=int(allowed))
        self.emit("assert_unused_choices")
        self.emit("assert_stack", value=int(allowed))
        self.mana("assert_mana", player, ZERO if allowed else fee)
        if allowed:
            self.resolve()
            types = self.oracle[name]["type_line"].partition(" — ")[0].split()
            destination = GRAVEYARD if {"Instant", "Sorcery"}.intersection(types) else BATTLEFIELD
            self.objects[ref][2] = destination

    def cast_source(self):
        self.emit("turn", player=self.owner)
        self.fund(self.foe)
        fee = RAINBOW if self.mode == "rainbow" else None
        denied = self.mode == "missing_blue"
        if denied:
            fee = (0, 1, 0, 1, 1, 1)
        self.cast("source", fee=fee, allowed=not denied)
        self.observe("source", "in_play" if not denied else "in_hand", 1)
        if denied:
            self.fund(self.owner)
        if self.id == BRINGER and not denied:
            self.observe("source", "power", 5)
            self.observe("source", "toughness", 5)

    def upkeep(self, player, *, optional=False, accept=True):
        assert player == 1 - self.active
        self.emit("advance_turn")
        self.emit("untap_step", player=player)
        if optional and accept and player == 0:
            self.emit("choose_trigger_source", "source", player=0)
        self.emit("upkeep_step", player=player)
        self.resolve()
        self.emit("turn", player=player)
        self.active = player

    def own_upkeep(self, *, optional=False, accept=True):
        self.upkeep(self.foe)
        self.state()
        self.upkeep(self.owner, optional=optional, accept=accept)

    def token_ref(self, index=0):
        self.emit("bind_permanent_copy", "token", player=self.owner,
                  index=index, value=TOKEN[self.token][0])
        return "token"

    def state(self, pools=(ZERO, ZERO)):
        cid, color, power, toughness, subtypes = TOKEN[self.token]
        for player in (0, 1):
            self.emit("assert_life", player=player, value=self.life[player])
            self.mana("assert_mana", player, pools[player])
            for zone in (HAND, BATTLEFIELD, LIBRARY, GRAVEYARD, EXILE):
                things = Counter(name for name, seat, where in self.objects.values()
                                 if seat == player and where == zone)
                if zone == BATTLEFIELD and self.tokens[player]:
                    things[self.token] += self.tokens[player]
                self.emit("assert_zone", player=player, zone=zone, value=things.total())
                for name, count in sorted(things.items()):
                    self.emit("assert_zone_multiplicity", player=player, zone=zone,
                              index=self.rows[name]["csvid"], value=count)
                if zone == BATTLEFIELD and not self.tokens[player]:
                    self.emit("assert_zone_multiplicity", player=player, zone=zone, index=cid)
            if self.tokens[player]:
                facts = [("token", 1), ("color", color), ("artifact", 0),
                         ("power", power), ("toughness", toughness)]
                facts.extend((f"subtype_{subtype}", 1) for subtype in subtypes)
                for prop, value in facts:
                    self.emit("assert_matching_permanents", prop=prop, player=player, index=cid, value=value)

    def combat(self, attacker, *, block=None, block_legal=True, hit=0):
        self.emit("begin_attackers")
        self.emit("attack", attacker, value=1)
        self.emit("finish_attackers")
        self.resolve()
        self.emit("begin_blockers")
        if block:
            self.emit("block", block, attacker, value=int(block_legal))
        self.emit("advance_combat")
        self.resolve()
        if block:
            self.observe(attacker, "blocked", int(block_legal))
        self.emit("combat_damage")
        self.resolve()
        self.emit("end_combat")
        self.life[self.foe] -= hit

    def remove_source(self, *, optional=False):
        self.cast("remove", target="source")
        self.objects["source"][2] = GRAVEYARD
        self.state()
        self.own_upkeep(optional=optional, accept=False)
        self.state()

    def legion(self):
        self.own_upkeep()
        self.tokens[self.owner] = 1
        self.state()
        if self.mode == "haste":
            self.combat(self.token_ref(), hit=1)
            self.state()
        elif self.mode == "removed":
            self.remove_source()
        else:
            self.own_upkeep()
            self.tokens[self.owner] += 2
            self.state()
            if self.mode == "reset":
                self.cast("snap")
                self.tokens[self.owner] = 0
                self.state()
                self.own_upkeep()
                self.tokens[self.owner] = 1
            else:
                self.own_upkeep()
                self.tokens[self.owner] += 3
            self.state()

    def zone(self):
        accept = self.mode != "decline"
        self.own_upkeep(optional=True, accept=accept)
        self.tokens[self.owner] += int(accept)
        self.state()
        if self.mode == "removed":
            self.remove_source(optional=True)
        if self.mode == "spawn_mana":
            token = self.token_ref()
            self.emit("can_activate", token, value=1)
            self.emit("activate", token, value=1)
            self.emit("refresh")
            self.emit("assert_stack")
            self.tokens[self.owner] = 0
            one = (1, 0, 0, 0, 0, 0)
            self.state((one, ZERO) if self.owner == 0 else (ZERO, one))
            self.cast("receiver", already_funded=True)
            self.state()

    def blossom(self):
        if self.mode == "kindred":
            self.cast("pepper", target="blocker")
            self.objects["top"][2] = HAND
            self.observe("blocker", "power", 1)
            self.observe("blocker", "toughness", 1)
            self.state()
        self.own_upkeep()
        self.life[self.owner] -= 1
        self.tokens[self.owner] = 1
        self.state()
        if self.mode == "removed":
            self.remove_source()
        elif self.mode in ("repeat", "flying"):
            self.own_upkeep()
            self.life[self.owner] -= 1
            self.tokens[self.owner] += 1
            self.state()
            if self.mode == "flying":
                self.combat(self.token_ref(), block="blocker", block_legal=False, hit=1)
                self.state()

    def bringer(self):
        accept = self.mode not in ("decline", "missing_blue")
        self.own_upkeep(optional=True, accept=accept)
        self.tokens[self.owner] = int(accept)
        self.state()
        if self.mode == "trample":
            self.combat("source", block="blocker", hit=3)
            self.observe("source", "damage", 2)
            self.objects["blocker"][2] = GRAVEYARD
            self.state()

    def run(self):
        self.cast_source()
        self.state()
        {LEGION: self.legion, ZONE: self.zone, BLOSSOM: self.blossom, BRINGER: self.bringer}[self.id]()


LIEGE, BROOD, OUTCAST, DRUID, RESPONSE, FOOTSTEPS, ASSAULT = 9759, 10263, 10755, 12520, 14100, 7849, 9976
PASTURES, RANKS, KEMBA, ANGEL, HUNT, SERGEANT, OPHIOMANCER = 3789, 12741, 11288, 6717, 10444, 10175, 13630
FOLLOW_MODES = {
    LIEGE: ('colors','decline'),
    BROOD: ('each','devour','flying'),
    OUTCAST: ('cross_six','five','flying'),
    DRUID: ('bare','enchanted','remove_aura'),
    RESPONSE: ('own_loss','foe_loss','next_clean'),
    FOOTSTEPS: ('copy_foe','copy_own','original_removed'),
    ASSAULT: ('own_required','foe_required','non_goblin'),
    PASTURES: ('own_more','foe_more','tie','flip_lead'),
    RANKS: ('copy','foe_token','none'),
    KEMBA: ('zero','two','move_equipment'),
    ANGEL: ('spirit_flying','source_flying','decline'),
    HUNT: ('pack','single_wolf','tapped_wolf'),
    SERGEANT: ('copy','foe_blue','trample'),
    OPHIOMANCER: ('both_upkeeps','own_snake','foe_snake','touch'),
}
ALL_MODES = MODES | FOLLOW_MODES
SINGLE_SEAT = HUMAN_ONLY | {(LIEGE,'decline'), (ANGEL,'decline'), (BROOD,'each'), (BROOD,'devour')}

WORM, DRAGON, SAPROLING, GOBLIN, CAT, SPIRIT, WOLF, SNAKE, GIANT = (
    'Worm','Dragon','Saproling','Goblin','Cat','Spirit','Wolf','Snake','Hill Giant')
FOLLOW_TOKENS = {
    WORM:(18858,10,3,3,()), SAPROLING:(18813,8,1,1,('Saproling',)),
    GOBLIN:(18746,16,1,1,('Goblin',)), CAT:(18704,32,2,2,('Cat',)),
    SPIRIT:(18829,32,1,1,('Spirit',)), WOLF:(18856,8,2,2,('Wolf',)),
    SNAKE:(18821,2,1,1,('Snake',)), GIANT:(110,16,3,3,()),
}
CAD, BLACK, DUAL, AURA, BOLT = 'Eager Cadet','Walking Corpse','Putrid Leech','Flight','Lightning Bolt'
BOUNCE, RED_GOBLIN, ALARM, SCIMITAR = 'Unsummon',"Mons's Goblin Raiders",'Raise the Alarm','Leonin Scimitar'
BLUE, SNAKE_CARD, ICY = 'Air Elemental','Orochi Sustainer','Icy Manipulator'
WITNESSES |= {*FOLLOW_TOKENS,DRAGON,CAD,BLACK,DUAL,AURA,BOLT,BOUNCE,RED_GOBLIN,ALARM,SCIMITAR,BLUE,SNAKE_CARD,ICY}


class FollowBoard(Board):
    def __init__(self,row,mode,owner,rows,oracle):
        self.id,self.name = row['csvid'],row['name']
        self.mode,self.owner,self.foe = mode,owner,1-owner
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.objects = [],[],{}
        self.life,self.active,self.tokens = [20,20],owner,Counter()
        self.token,self.traits = self.token_spec()
        self.add('source',self.name,owner,HAND)
        self.witnesses()

    def token_spec(self):
        name = {
            LIEGE:WORM, BROOD:DRAGON, OUTCAST:DRAGON, DRUID:SAPROLING, RESPONSE:SOLDIER,
            FOOTSTEPS:GIANT, ASSAULT:GOBLIN, PASTURES:SAPROLING, RANKS:SOLDIER, KEMBA:CAT,
            ANGEL:SPIRIT, HUNT:WOLF, SERGEANT:BEAST, OPHIOMANCER:SNAKE,
        }[self.id]
        if self.id==BROOD:
            size = 3 if self.mode=='devour' else 1
            return name,(18719,24,size,size,('Dragon',))
        if self.id==OUTCAST:
            return name,(18719,16,5,5,('Dragon',))
        if name==SOLDIER:
            return name,(18822,32,1,1,('Soldier',))
        if name==BEAST:
            return name,TOKEN[name]  # Sergeant’s copies share its own CSVID and use the ordinary ledger.
        return name,FOLLOW_TOKENS[name]

    def witnesses(self):
        p,q,m = self.owner,self.foe,self.mode
        if self.id==LIEGE:
            for ref,name,holder in (('black',BLACK,p),('green',BEAR,p),('dual',DUAL,p),('foreign',DUAL,q)):
                self.add(ref,name,holder,BATTLEFIELD)
        elif self.id==BROOD:
            if m=='devour':
                self.add('victim',CAD,p,BATTLEFIELD)
                self.add('enemy',CAD,q,BATTLEFIELD)
            if m=='flying':
                self.add('blocker',WALL,q,BATTLEFIELD)
        elif self.id==OUTCAST:
            for i in range(5 if m!='flying' else 6):
                self.add(f'land_{i}',FOREST,p,BATTLEFIELD)
            self.add('enemy_land',FOREST,q,BATTLEFIELD)
            if m=='cross_six':
                self.add('sixth',FOREST,p,HAND)
            if m=='flying':
                self.add('blocker',WALL,q,BATTLEFIELD)
        elif self.id==DRUID:
            if m!='bare':
                self.add('aura',AURA,p,HAND)
            if m=='remove_aura':
                self.add('remove',DISENCHANT,q,HAND)
        elif self.id==RESPONSE:
            self.add('bolt',BOLT,p if m=='foe_loss' else q,HAND)
        elif self.id==FOOTSTEPS:
            self.add('original',GIANT,q if m=='copy_foe' else p,BATTLEFIELD)
            if m!='copy_foe':
                self.add('bounce',BOUNCE,q,HAND)
        elif self.id==ASSAULT:
            if m!='own_required':
                self.add('witness',CAD if m=='non_goblin' else RED_GOBLIN,q,BATTLEFIELD)
        elif self.id==PASTURES:
            both = (2,1) if m in ('own_more','flip_lead') else (1,2) if m=='foe_more' else (1,1)
            for seat,count in zip((p,q),both):
                for i in range(count):
                    self.add(f'land_{seat}_{i}',FOREST,seat,BATTLEFIELD)
            if m=='flip_lead':
                self.add('first',FOREST,q,HAND)
                self.add('second',FOREST,q,HAND)
        elif self.id==RANKS:
            self.add('non_token',CAD,p,BATTLEFIELD)
            if m!='none':
                self.add('alarm',ALARM,p if m=='copy' else q,HAND)
        elif self.id==KEMBA:
            self.add('eligible',CAD,p,BATTLEFIELD)
            self.add('one',BONES,p,BATTLEFIELD)
            self.add('two',SCIMITAR,p,BATTLEFIELD)
        elif self.id==ANGEL:
            if m.endswith('_flying'):
                self.add('blocker',WALL,q,BATTLEFIELD)
        elif self.id==HUNT:
            self.add('own_other',BEAR,p,BATTLEFIELD)
            if m!='pack':
                self.add('prey',CAD,q,BATTLEFIELD)
            if m=='tapped_wolf':
                self.add('icy',ICY,p,BATTLEFIELD)
        elif self.id==SERGEANT:
            if m!='trample':
                self.add('blue',BLUE,p if m=='copy' else q,BATTLEFIELD)
            else:
                self.add('blocker',BEAR,q,BATTLEFIELD)
        elif self.id==OPHIOMANCER:
            if m.endswith('_snake'):
                self.add('snake',SNAKE_CARD,p if m=='own_snake' else q,BATTLEFIELD)
            if m=='touch':
                self.add('blocker',WALL,q,BATTLEFIELD)

    def state(self,pools=(ZERO,ZERO),*,stats=True):
        cid,color,power,toughness,subtypes = self.traits
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            self.mana('assert_mana',player,pools[player])
            for zone in range(5):
                things = Counter(name for name,seat,where in self.objects.values()
                                 if seat==player and where==zone)
                if zone==BATTLEFIELD and self.tokens[player]:
                    things[self.token] += self.tokens[player]
                self.emit('assert_zone',player=player,zone=zone,value=things.total())
                for name,count in sorted(things.items()):
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,
                              value=count,index=self.rows[name]['csvid'])
            if not self.tokens[player] or not stats:
                continue
            assert not any(name==self.token and p==player and z==BATTLEFIELD for name,p,z in self.objects.values())
            facts = [('token',1),('color',color),('artifact',0),('power',power),('toughness',toughness)]
            facts += [('subtype_'+kind,1) for kind in subtypes]
            for prop,value in facts:
                self.emit('assert_matching_permanents',prop=prop,player=player,index=cid,value=value)

    def cast_primary(self):
        self.emit('turn',player=self.owner)
        self.fund(self.foe)
        alternate = {LIEGE:(1,3,0,0,0,0),RANKS:(2,0,0,0,0,2)}
        self.cast('source',fee=alternate.get(self.id),target='original' if self.id==FOOTSTEPS else None)
        self.observe('source','in_play',1)
        self.state()

    def phase(self, seat, *, choices=None):
        assert seat==1-self.active
        self.emit('advance_turn')
        self.emit('untap_step',player=seat)
        if choices:
            choices()
        self.emit('upkeep_step',player=seat)
        self.resolve()
        self.emit('turn',player=seat)
        self.active=seat

    def play_land(self,ref):
        player = self.objects[ref][1]
        assert player==self.active and self.objects[ref][2]==HAND
        self.emit('play_land',ref,value=1)
        self.objects[ref][2]=BATTLEFIELD
        self.state()

    def bind(self,ref,name,player,index=0):
        self.emit('bind_permanent_copy',ref,player=player,index=index,value=self.rows[name]['csvid'])

    def stats(self,ref,power,toughness=None):
        self.observe(ref,'power',power)
        self.observe(ref,'toughness',power if toughness is None else toughness)

    def change(self,ref,zone):
        self.objects[ref][2]=zone

    def optional_choice(self):
        if self.owner==0:
            self.emit('choose_trigger_source','source',player=0)

    def loop_pair(self):
        self.phase(self.foe)
        self.state()
        self.phase(self.owner)

    def liege(self):
        p,m = self.owner,self.mode
        self.stats('source',2)
        self.stats('black',3)
        self.stats('green',3)
        self.stats('dual',4)
        self.stats('foreign',2)
        self.own_upkeep(optional=True,accept=m!='decline')
        self.tokens[p]=int(m!='decline')
        self.state()

    def broodmother(self):
        p,q,m = self.owner,self.foe,self.mode
        if m=='flying':
            for seat in (q,p):
                self.emit('advance_turn')
                self.emit('untap_step',player=seat)
                self.emit('turn',player=seat)
            self.combat('source',block='blocker',block_legal=False,hit=4)
            self.state()
            return
        def devour():
            if m=='devour':
                self.emit('choose_target','victim',player=p)
            self.emit('choose_cancel_target',player=p)
        self.phase(q,choices=devour)
        if m=='devour':
            self.change('victim',GRAVEYARD)
        self.tokens[p]=1
        self.state()
        if m=='devour':
            self.bind('dragon',DRAGON,p)
            self.observe('dragon','plus1_counters',2)
        else:
            self.phase(p,choices=devour)
            self.tokens[p]=2
            self.state()

    def outcast(self):
        p,q,m = self.owner,self.foe,self.mode
        self.loop_pair()
        if m=='flying':
            self.tokens[p]=1
            self.state()
            self.loop_pair()
            self.tokens[p]=2
            self.bind('dragon',DRAGON,p)
            self.combat('dragon',block='blocker',block_legal=False,hit=5)
        elif m=='cross_six':
            self.state()
            self.play_land('sixth')
            self.loop_pair()
            self.tokens[p]=1
        self.state()

    def druid(self):
        p,q,m = self.owner,self.foe,self.mode
        if m!='bare':
            self.cast('aura',target='source')
        self.phase(q)
        self.tokens[p]=int(m!='bare')
        self.state()
        self.phase(p)
        self.tokens[p]+=int(m!='bare')
        self.state()
        if m=='remove_aura':
            self.cast('remove',target='aura')
            self.change('aura',GRAVEYARD)
            self.loop_pair()
            self.state()

    def response(self):
        p,q,m = self.owner,self.foe,self.mode
        seat = q if m=='foe_loss' else p
        self.fund(1-seat,cost_from_oracle(self.oracle[BOLT]))
        self.emit('choose_player',player=1-seat,value=seat)
        self.cast('bolt',already_funded=True)
        self.life[seat]-=3
        self.phase(q)
        self.tokens[p]=int(seat==p)
        self.state()
        if m=='next_clean':
            self.phase(p)
            self.state()

    def footsteps(self):
        p,q,m = self.owner,self.foe,self.mode
        if m=='original_removed':
            self.cast('bounce',target='original')
            self.change('original',HAND)
            self.change('source',GRAVEYARD)
        self.phase(q)
        self.state()
        self.phase(p)
        if m=='copy_own':
            self.tokens[p]=1
            self.state(stats=False)
            self.cast('bounce',target='original')
            self.change('original',HAND)
            self.change('source',GRAVEYARD)
        elif m=='copy_foe':
            self.tokens[p]=1
        self.state()

    def assault(self):
        p,q,m = self.owner,self.foe,self.mode
        if m=='own_required':
            self.loop_pair()
            self.tokens[p]=1
            self.bind('attacker',GOBLIN,p)
            active,life_loss = p,q
        else:
            self.phase(q)
            self.state()
            active,life_loss = q,p
        attacker = 'attacker' if active==p else 'witness'
        self.emit('begin_required_attackers')
        self.emit('can_finish_attackers',value=int(m=='non_goblin'))
        self.emit('attack',attacker,value=1)
        self.emit('can_finish_attackers',value=1)
        self.emit('finish_attackers')
        self.resolve()
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.life[life_loss]-=1
        self.state()

    def pastures(self):
        p,q,m = self.owner,self.foe,self.mode
        self.phase(q)
        self.tokens[q]=int(m=='foe_more')
        self.state()
        if m=='flip_lead':
            self.play_land('first')
        self.phase(p)
        self.tokens[p]=int(m=='own_more')
        self.state()
        if m=='flip_lead':
            self.phase(q)
            self.state()
            self.play_land('second')
            self.phase(p)
            self.state()
            self.phase(q)
            self.tokens[q]=1
            self.state()

    def ranks(self):
        p,q,m = self.owner,self.foe,self.mode
        if m!='none':
            self.cast('alarm')
            self.tokens[p if m=='copy' else q]=2
        self.phase(q)
        self.state()
        if m=='copy':
            self.bind('original_token',SOLDIER,p)
        def choose():
            if m=='copy' and p==0:
                self.emit('choose_target','original_token',player=p)
        self.phase(p,choices=choose)
        self.tokens[p]+=int(m=='copy')
        self.state()

    def equipment(self,ref,recipient):
        p = self.owner
        self.fund(p,(1,0,0,0,0,0))
        self.emit('choose_target',recipient,player=p)
        self.emit('activate',ref,value=1)
        self.emit('assert_stack',value=1)
        self.mana('assert_mana',p)
        self.resolve()

    def kemba(self):
        p,m = self.owner,self.mode
        if m!='zero':
            self.equipment('one','source')
            self.equipment('two','source')
            self.stats('source',5,5)
        if m=='move_equipment':
            self.equipment('one','eligible')
            self.stats('source',3,5)
            self.stats('eligible',3,1)
        self.loop_pair()
        self.tokens[p]={'zero':0,'two':2,'move_equipment':1}[m]
        self.state()

    def angel(self):
        p,m = self.owner,self.mode
        self.own_upkeep(optional=True,accept=m!='decline')
        self.tokens[p]=int(m!='decline')
        self.state()
        if m=='source_flying':
            self.combat('source',block='blocker',block_legal=False,hit=4)
        if m=='spirit_flying':
            self.own_upkeep(optional=True)
            self.tokens[p]=2
            self.bind('spirit',SPIRIT,p)
            self.combat('spirit',block='blocker',block_legal=False,hit=1)
        self.state()

    def hunt(self):
        p,q,m = self.owner,self.foe,self.mode
        self.loop_pair()
        self.tokens[p]=1
        self.state()
        if m=='pack':
            self.loop_pair()
            self.tokens[p]=2
            self.state()
            return
        self.bind('wolf',WOLF,p)
        tapped = m=='tapped_wolf'
        if tapped:
            self.fund(p,(1,0,0,0,0,0))
            self.emit('choose_target','wolf',player=p)
            self.emit('activate','icy',value=1)
            self.mana('assert_mana',p)
            self.resolve()
            self.observe('wolf','tapped',1)
        self.emit('choose_target','prey',player=p)
        self.emit('activate','source',value=1)
        self.emit('assert_stack',value=1)
        self.observe('source','tapped',1)
        if p==0:
            self.emit('choose_dialog',player=q,value=0)
        self.resolve()
        self.observe('wolf','tapped',1)
        self.observe('wolf','damage',0 if tapped else 1)
        self.observe('own_other','tapped',0)
        if not tapped:
            self.change('prey',GRAVEYARD)
        self.state()

    def sergeant(self):
        p,q,m = self.owner,self.foe,self.mode
        self.own_upkeep(optional=True,accept=m=='copy')
        if m=='copy':
            self.objects['copy']=[self.name,p,BATTLEFIELD]
            self.bind('copy',self.name,p,index=1)
            self.stats('copy',4)
            self.observe('copy','sick',1)
        if m=='trample':
            self.combat('source',block='blocker',hit=2)
            self.change('blocker',GRAVEYARD)
            self.observe('source','damage',2)
        self.state()

    def ophiomancer(self):
        p,q,m = self.owner,self.foe,self.mode
        self.phase(q)
        self.tokens[p]=int(m!='own_snake')
        self.state()
        self.phase(p)
        self.state()
        if m=='touch':
            self.bind('token',SNAKE,p)
            self.combat('token',block='blocker')
            self.change('blocker',GRAVEYARD)
            self.state()
        if m=='both_upkeeps':
            self.phase(q)
            self.state()

    def run(self):
        self.cast_primary()
        {
            LIEGE:self.liege, BROOD:self.broodmother, OUTCAST:self.outcast, DRUID:self.druid,
            RESPONSE:self.response, FOOTSTEPS:self.footsteps, ASSAULT:self.assault, PASTURES:self.pastures,
            RANKS:self.ranks, KEMBA:self.kemba, ANGEL:self.angel, HUNT:self.hunt,
            SERGEANT:self.sergeant, OPHIOMANCER:self.ophiomancer,
        }[self.id]()


GAPS = {
    LEGION: ['Muster is observed through exact token increments and a real all-counter reset.'],
    ZONE: ['Decline uses the seat that receives an optional priority prompt.'],
    BLOSSOM: ['The Faerie Kindred identity is tested by casting Peppersmoke before the first token exists.'],
    BRINGER: ['Decline uses the seat that receives an optional priority prompt.'],
    LIEGE: ['Worm uses the exact generated identity. The direct subtype observer lacks Worm.'],
    BROOD: ['Devour selection uses seat 0. The seat-1 sacrifice helper can return a candidate without initializing target coordinates. Flying is tested in both seats.'],
    FOOTSTEPS: ['Tests copy an ordinary printed creature, then independently verify attachment loss and copy retention.'],
    HUNT: ['Damage division has a unique allocation with one untapped Wolf. The manual many-Wolf allocation is not exposed symmetrically by the legacy handler.'],
    SERGEANT: ['The first generation copies the source. Propagation through a later generation is outside this recipe.'],
}

ZONES = ('hand','battlefield','library','graveyard','exile')
ZONE_OPS = {'assert_zone','assert_zone_multiplicity'}
INDEXED = {'assert_zone_multiplicity','assert_matching_permanents','bind_permanent_copy'}
SEATED = ZONE_OPS | INDEXED | {
    'set_mana','assert_mana','assert_life','turn','untap_step','upkeep_step','choose_trigger_source',
    'choose_cancel_target','choose_target','choose_player','choose_dialog',
}
VALUED = ZONE_OPS | INDEXED | {
    'set_mana','assert_mana','assert_life','turn','assert_stack','assert_card','assert_unused_choices','cast',
    'can_activate','activate','attack','block','can_finish_attackers','play_land','choose_player','choose_dialog',
}


def operation(row):
    op,ref,other,prop,player,value,zone,index,color = row
    step = dict(op=op)
    if ref:
        step['ref'] = ref
    if other:
        step['other'] = other
    if prop:
        step['property'] = prop
    if op in SEATED:
        step['player'] = player
    if op in VALUED:
        step['value'] = value
    if op in ZONE_OPS:
        step['zone'] = ZONES[zone]
    if op in INDEXED:
        step['index'] = index
    if op in ('set_mana','assert_mana'):
        step['color'] = color
    return step


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def exact(actual,pinned):
    return json.dumps(actual,sort_keys=True,ensure_ascii=False)==json.dumps(pinned,sort_keys=True,ensure_ascii=False)


def classify(record,csvid):
    pin = profiles()['sources'].get(record.get('name'))
    if (not pin or not exact(record,pin['oracle']) or csvid not in ALL_MODES
            or pin['registry']['csvid']!=csvid or record.get('layout')!='normal'
            or record.get('selected_face') or record.get('card_faces')):
        raise ValueError('whole pinned normal upkeep-token primary required')
    return dict(csvid=csvid,mana_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{seat}' for mode in ALL_MODES[csvid]
            for seat in ((0,) if (csvid,mode) in SINGLE_SEAT else (0,1))]


def validate_variants(shape,selected):
    if (type(selected) is not list or not selected or any(type(mode) is not str for mode in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct supported upkeep-token variants required')


def spec(declaration,record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin:
        raise ValueError('pinned upkeep-token primary required')
    shape = classify(record,pin['registry']['csvid'])
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact declared upkeep-token parameters required')
    validate_variants(shape,declaration.get('variants'))
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pinned = profiles()
    if (set(pinned)!={'sources','witnesses'} or set(pinned['witnesses'])!=WITNESSES
            or {pin['registry']['csvid'] for pin in pinned['sources'].values()}!=ALL_MODES.keys()):
        raise ValueError('complete upkeep-token source and witness pins required')
    generated = (TOKEN | FOLLOW_TOKENS | {DRAGON:(18719,)})
    generated.pop(GIANT)
    for group,entries in pinned.items():
        for name,pin in entries.items():
            record,row = oracle.get(name),rows.get(name,{})
            if not exact(record,pin['oracle']) or not exact(row,pin['registry']):
                raise ValueError('upkeep-token source or witness changed: '+name)
            if group=='sources':
                if row.get('kind')!='card' or row.get('handler') in (None,'0','NULL','card_dummy'):
                    raise ValueError('implemented upkeep-token primary required')
                classify(record,row['csvid'])
            elif name in generated:
                if record['layout']!='token' or row['csvid']!=generated[name][0]:
                    raise ValueError('exact token identity required: '+name)
            elif record['layout']!='normal' or row['kind']!='card':
                raise ValueError('normal typed upkeep-token witness required: '+name)


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or shape['csvid']!=csvid:
        raise ValueError('exact registered upkeep-token source required')
    validate_variants(shape,selected)
    validate_witnesses(oracle,rows)
    result = []
    names = {rows[n]['csvid']:n for n in WITNESSES|{name}}
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        primary = rows[name]
        board = (FollowBoard if csvid in FOLLOW_MODES else Board)(primary,mode,int(seat),rows,oracle)
        board.run()
        setup = [dict(name=names[cid],ref=ref,player=player,zone=ZONES[zone])
                 for ref,player,zone,cid,flags in board.setup]
        result.append(dict(name=label(csvid,mode,seat),card=name,fixture='empty_main',seed=SEED,
                           setup=setup,steps=[operation(step) for step in board.steps],
                           _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                        oracle_text=declaration['oracle_text'],untested_components=GAPS.get(csvid,[]))))
    return result
