"""Paid departure effects, last known counters and library choices."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counter_condition_recurrence_recipes import exact
from recipes import card, cost_from_oracle, step

FAMILY = 'departure_counters_and_search'
PROFILE = Path(__file__).with_name('data') / 'departure_counters_and_search_wave8_profiles.json'
WITCH, MANTLE, AURAMANCERS, NECROPEDE, INCARNATION, SU_CHI, SEER = 11883,3768,8964,11319,181,529,3312
VASTWOOD, VODALIAN, SKYCAPTAIN, RESHAPER, IMPLEMENT, KEEPER, SWARM = 13356,1843,14506,15638,16731,16815,5875
CURSE, ENCHANTMENT, SHADOW = 'Curse of the Bloody Tome','Fervor','Dauthi Slayer'
MERFOLK, SMALL, NATURALIZE = 'Merfolk of the Pearl Trident','Elvish Mystic','Naturalize'
BOUNCE_PERMANENT, BOLT = 'Boomerang','Lightning Bolt'

HYDRA, SNAPPER, MEADOW, SERVANT = 14471, 9590, 9383, 15014
MODES = {
    HYDRA: ('x_zero','x_one','pumped','trample','bounce','countered'),
    SNAPPER: ('spread','none','wither','wither_dies','bounce','exile','countered'),
    MEADOW: ('normal','target_foe','bounce','exile','evoke','evoke_countered'),
    SERVANT: ('transfer','larger','buffed','no_recipient','bounce','countered'),
}
ZONES = {'hand':0,'battlefield':1,'library':2,'graveyard':3,'exile':4}
ZERO = [0]*6
COUNTER, MURDER, SHATTER = 'Counterspell','Murder','Shatter'
BOUNCE, EXILE = 'Unsummon','Swords to Plowshares'
BEAR, SPIDER, WALL, ARTIFACT = 'Grizzly Bears','Giant Spider','Dazzling Ramparts','Millstone'
CLASP, GROWTH, HUNGER = 'Contagion Clasp','Giant Growth','Hunger of the Howlpack'
LIBRARY = ('Island','Forest','Plains','Mountain','Swamp','Wastes','Island','Forest')

FOLLOWERS = {11883: ('search', 'whiff', 'decline', 'combat', 'bounce'), 3768: ('pump', 'host_dies', 'destroy', 'bounce'), 8964: ('search', 'early', 'decline', 'whiff'), 11319: ('dies', 'decline', 'bounce', 'infect_player', 'infect_block'), 181: ('even', 'odd', 'bounce', 'proactive'), 529: ('dies', 'bounce', 'exile'), 3312: ('dies', 'bounce', 'exile', 'shadow', 'shadow_block'), 13356: ('split', 'single', 'decline', 'bounce'), 1843: ('foreign_only', 'reassign', 'pump', 'pump_twice', 'expiry'), 14506: ('weakest', 'tie', 'flying', 'bounce'), 15638: ('accept', 'decline', 'land', 'large', 'spell', 'empty'), 16731: ('ability', 'dies', 'bounce'), 16815: ('cast', 'decline', 'whiff', 'large'), 5875: ('split', 'three', 'single', 'bounce')}
MODES.update(FOLLOWERS)
HUMAN_ONLY = [(8964, 'decline'), (11319, 'decline'), (11883, 'decline'), (13356, 'decline'), (16815, 'decline')]
WITNESSES = frozenset(('Boomerang', 'Contagion Clasp', 'Counterspell', 'Curse of the Bloody Tome', 'Dauthi Slayer', 'Dazzling Ramparts', 'Elvish Mystic', 'Fervor', 'Forest', 'Giant Growth', 'Giant Spider', 'Grizzly Bears', 'Hunger of the Howlpack', 'Island', 'Lightning Bolt', 'Merfolk of the Pearl Trident', 'Millstone', 'Mountain', 'Murder', 'Naturalize', 'Plains', 'Shatter', 'Swamp', 'Swords to Plowshares', 'Unsummon', 'Wastes'))
ZONE_NAMES = {code:name for name,code in ZONES.items()}


GAPS = {
    WITCH: ('The Curse attachment is observed through the targeted upkeep mill and actual library/graveyard contents.',),
    INCARNATION: ('Proactive legality is observed through the ordinary activation predicate. The legacy requires a live damage packet; this driver does not independently inspect unspent redirection shields or activate the owner’s legacy on a stolen source.',),
    RESHAPER: ('The revealed identity is observed through its exact legal battlefield or hand destination.',),
    KEEPER: ('Single skipped cards test bottom placement exactly. Random order of multiple skipped cards is not constrained.',),
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record, csvid):
    if csvid not in MODES or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('normal battlefield-departure primary required')
    return dict(csvid=csvid, printed_cost=record['mana_cost'])


def variants(shape):
    csvid = shape['csvid']
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in MODES[csvid]
            if owner == 0 or (csvid,mode) not in HUMAN_ONLY]


def validate_variants(shape, chosen):
    if (type(chosen) is not list or not chosen or any(type(item) is not str for item in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('distinct supported departure variants required')


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('name'))
    if not pin or not exact(pin['oracle'], record):
        raise ValueError('exact pinned departure primary required')
    shape = classify(record, pin['registry']['csvid'])
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(declaration.get('params'), shape)):
        raise ValueError('exact typed departure declaration required')
    validate_variants(shape, declaration.get('variants'))
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if (set(pins) != {'sources','witnesses'} or set(pins['witnesses']) != WITNESSES
            or {item['registry']['csvid'] for item in pins['sources'].values()} != MODES.keys()):
        raise ValueError('exact departure sources and witnesses required')
    for group, entries in pins.items():
        for name, pin in entries.items():
            row = rows.get(name, {})
            if (not exact(oracle.get(name), pin['oracle']) or not exact(row, pin['registry'])
                    or row.get('kind') != 'card' or row.get('handler') in (None,'0','NULL')
                    or oracle[name].get('layout') != 'normal'):
                raise ValueError('departure Oracle or registry changed: ' + name)
            if group == 'sources':
                if row['handler'] == 'card_dummy':
                    raise ValueError('implemented departure primary required')
                classify(oracle[name], row['csvid'])


def expand(name, csvid, declaration, shape, chosen, rows, oracle):
    validate_variants(shape, chosen)
    validate_witnesses(oracle, rows)
    if shape['csvid'] != csvid or profiles()['sources'][name]['registry']['csvid'] != csvid:
        raise ValueError('exact departure primary CSVID required')
    cases = []
    for variant in chosen:
        mode, seat = variant.rsplit('_', 1)
        board = Board(rows[name], int(seat), mode, rows, oracle)
        board.run()
        cases.append(dict(name=f'DepartureCountersAndSearchRecipe_{csvid}_{variant}', card=name,
                          fixture='empty_main', seed=3596, setup=board.setup, steps=board.steps,
                          _recipe=dict(family=FAMILY, version=2, variant=variant, params=declaration['params'],
                                       oracle_text=declaration['oracle_text'], untested_components=list(GAPS.get(csvid, ())))))
    return cases

class Board:
    def __init__(self,record,owner,mode,rows,oracle):
        self.id,self.name = record['csvid'],record['name']
        self.owner,self.foe,self.mode = owner,1-owner,mode
        self.rows,self.oracle = rows,oracle
        self.setup,self.steps,self.cards = [],[],{}
        self.life = [20,20]
        self.wallet = [ZERO.copy(),ZERO.copy()]
        self.deck = [[],[]]
        self.add('source',self.name,owner,'hand')
        if mode.endswith('countered'):
            self.add('counter',COUNTER,self.foe,'hand')
        self.witnesses() if self.id not in FOLLOWERS else self.follower_setup()

    def add(self,ref,name,player,zone='battlefield'):
        assert ref not in self.cards
        self.setup.append(card(name,ref,player,zone))
        self.cards[ref] = dict(name=name,player=player,zone=zone)
        if zone=='library':
            self.deck[player].append(ref)

    def emit(self,op,ref='',other='',prop='',*,player=0,value=0,zone=0,index=0,color=0):
        if op in ('untap_step','upkeep_step'):
            self.steps.append(step(op,player=player))
            return
        if op == 'end_combat_step':
            self.steps.append(step(op,value=value))
            return
        if op == 'assert_shuffle_count':
            self.steps.append(step(op,player=player,value=value))
            return
        if op == 'assert_counter':
            self.steps.append(step(op,ref=ref,counter_kind=prop,value=value))
            return
        if op == 'choose_optional_target':
            self.steps.append(step(op,ref=ref,player=player))
            return
        if op == 'assert_forced_target_choices':
            self.steps.append(step(op,value=value))
            return
        values = dict(player=player,value=value,index=index,color=color)
        if ref:
            values['ref'] = ref
        if other:
            values['other'] = other
        if prop:
            values['property'] = prop
        if op.startswith('assert_zone'):
            values['zone'] = ZONE_NAMES[zone]
        self.steps.append(step(op,**values))

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref,prop=prop,value=value)

    def stats(self,ref,change=0):
        for prop in ('power','toughness'):
            printed = int(self.oracle[self.cards[ref]['name']][prop])
            self.observe(ref,prop,printed+change)

    def counters(self,ref,count,*,negative=False):
        self.emit('assert_counter',ref,prop='minus1' if negative else 'plus1',value=count)
        self.stats(ref,-count if negative else count)

    def move(self,ref,zone):
        self.cards[ref]['zone'] = zone

    def pool(self,player,amount,*,fund=False):
        for color,value in enumerate(amount):
            self.emit('set_mana' if fund else 'assert_mana',player=player,value=value,color=color)
        if fund:
            self.wallet[player] = list(amount)
            self.emit('refresh')

    def choices(self):
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_choices_consumed')

    def announce(self,ref,*,target=None,response=False,printed=None,mode=None,x=None):
        player = self.cards[ref]['player']
        record = self.oracle[self.cards[ref]['name']]
        if printed is None:
            printed = record['mana_cost']
        fixed = printed.replace('{X}','')
        colorless = fixed.count('{C}')
        fee = cost_from_oracle(dict(mana_cost=fixed.replace('{C}','')))
        fee[0] += colorless
        if x is not None:
            fee[0] += x
        self.pool(player,fee,fund=True)
        if x is not None:
            self.emit('choose_x',player=player,value=x)
        if mode is not None:
            self.emit('choose_dialog',player=player,value=mode)
        if target:
            self.emit('choose_target',target,player=player)
        self.emit('cast_response' if response else 'cast',ref,value=1)
        self.choices()
        self.emit('assert_stack',value=2 if response else 1)
        self.wallet[player] = ZERO.copy()
        self.pool(player,ZERO)

    def resolve(self,*,response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.choices()
        self.emit('assert_stack')

    def receipt(self):
        self.emit('assert_stack')
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            self.pool(player,self.wallet[player])
            for zone,code in ZONES.items():
                entries = Counter(item['name'] for item in self.cards.values()
                                  if item['player']==player and item['zone']==zone)
                self.emit('assert_zone',player=player,zone=code,value=entries.total())
                for name,count in sorted(entries.items()):
                    self.emit('assert_zone_multiplicity',player=player,zone=code,
                              index=self.rows[name]['csvid'],value=count)
            for index,ref in enumerate(self.deck[player]):
                self.emit('assert_zone_card',player=player,zone=ZONES['library'],
                          index=index,value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref,item in self.cards.items():
            if item['zone']=='battlefield' and not ref.startswith('deck_'):
                self.observe(ref,'in_play',1)

    def draw(self,count):
        deck = self.deck[self.owner]
        assert len(deck) > count
        for ref in deck[:count]:
            self.move(ref,'hand')
        del deck[:count]
        self.life[self.owner] += count

    def visit(self,seat):
        assert self.wallet==[ZERO,ZERO]
        self.emit('advance_turn')
        self.emit('untap_step',player=seat)
        self.emit('turn',player=seat)

    def attack(self,blocker):
        self.visit(self.foe)
        self.visit(self.owner)
        self.emit('begin_attackers')
        self.emit('attack','source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',blocker,'source',value=1)
        self.emit('advance_combat')
        self.observe('source','blocked',1)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat_step')
        self.resolve()

    def exit_source(self,*,target=None,target_player=None,power=None):
        self.announce('exit',target='source')
        if target is not None:
            self.emit('choose_target',target,player=self.owner)
        if target_player is not None:
            self.emit('choose_player',player=self.owner,value=target_player)
        self.resolve()
        spell = self.cards['exit']['name']
        zone = 'hand' if spell==BOUNCE else 'exile' if spell==EXILE else 'graveyard'
        self.move('source',zone)
        self.move('exit','graveyard')
        if spell==EXILE:
            assert power is not None
            self.life[self.owner] += power

    def witnesses(self):
        mode = self.mode
        if self.id == HYDRA:
            for seat,names in ((self.owner,LIBRARY),(self.foe,LIBRARY[:2])):
                for position,name in enumerate(names):
                    self.add(f'deck_{seat}_{position}',name,seat,'library')
            if mode=='trample':
                self.add('blocker',BEAR,self.foe)
            if mode=='pumped':
                self.add('growth',GROWTH,self.owner,'hand')
            if mode in ('x_one','pumped','bounce'):
                self.add('exit',BOUNCE if mode=='bounce' else MURDER,self.foe,'hand')
        elif self.id == SNAPPER:
            for ref,who in (('own_countered',self.owner),('foe_countered',self.foe)):
                self.add(ref,SPIDER,who)
            self.add('clean_bear',BEAR,self.owner)
            if mode in ('spread','bounce','exile'):
                for index in range(2):
                    self.add(f'clasp_{index}',CLASP,self.owner,'hand')
            if mode in ('spread','none','bounce','exile'):
                death = {'bounce':BOUNCE,'exile':EXILE}.get(mode,SHATTER)
                self.add('exit',death,self.foe,'hand')
            if mode=='wither':
                self.add('wall',WALL,self.foe)
        elif self.id == MEADOW:
            for seat in (self.owner,self.foe):
                self.add(f'bear_{seat}',BEAR,seat)
                self.add(f'spider_{seat}',SPIDER,seat)
                self.add(f'artifact_{seat}',ARTIFACT,seat)
            if mode in ('normal','target_foe','bounce','exile'):
                self.add('exit',{'bounce':BOUNCE,'exile':EXILE}.get(mode,MURDER),self.foe,'hand')
        elif self.id == SERVANT:
            if mode!='no_recipient':
                self.add('target',BEAR,self.owner)
                self.add('other_ally',SPIDER,self.owner)
            self.add('opposing',BEAR,self.foe)
            if mode=='larger':
                self.add('change',HUNGER,self.owner,'hand')
            if mode=='buffed':
                self.add('change',GROWTH,self.owner,'hand')
            if mode!='countered':
                self.add('exit',BOUNCE if mode=='bounce' else MURDER,self.foe,'hand')

    def countered(self,**kwargs):
        self.announce('source',**kwargs)
        self.emit('announce_spell','source')
        self.announce('counter',response=True)
        self.resolve(response=True)
        self.move('source','graveyard')
        self.move('counter','graveyard')
        self.receipt()

    def hydra(self):
        x = {'x_zero':0,'x_one':1,'trample':4}.get(self.mode,3)
        if self.mode=='countered':
            self.countered(x=x)
            return
        self.announce('source',x=x)
        self.resolve()
        self.move('source','battlefield' if x else 'graveyard')
        if x:
            self.counters('source',x)
        self.receipt()
        if not x:
            return
        if self.mode=='trample':
            self.attack('blocker')
            self.life[self.foe] -= x-2
            self.move('blocker','graveyard')
            self.counters('source',x)
        else:
            bonus = 0
            if self.mode=='pumped':
                self.announce('growth',target='source')
                self.resolve()
                self.move('growth','graveyard')
                self.emit('assert_counter','source',prop='plus1',value=x)
                self.stats('source',x+3)
                bonus = 3
            self.exit_source()
            if self.mode!='bounce':
                self.draw(x+bonus)
        self.receipt()

    def snapper(self):
        if self.mode=='countered':
            self.countered()
            return
        self.announce('source')
        self.resolve()
        self.move('source','battlefield')
        self.stats('source')
        if self.mode in ('wither','wither_dies'):
            recipient = 'wall' if self.mode=='wither' else 'foe_countered'
            self.attack(recipient)
            dies = self.mode=='wither_dies'
            if dies:
                self.move('source','graveyard')
            self.counters(recipient,2+int(dies),negative=True)
            self.observe(recipient,'damage',0)
            self.counters('own_countered',0,negative=True)
        else:
            seeding = self.mode!='none'
            for idx,target in enumerate(('own_countered','foe_countered')):
                if seeding:
                    ref = f'clasp_{idx}'
                    self.announce(ref)
                    self.emit('choose_target',target,player=self.owner)
                    self.resolve()
                    self.move(ref,'battlefield')
                self.counters(target,int(seeding),negative=True)
            self.counters('clean_bear',0,negative=True)
            self.receipt()
            self.exit_source(power=2)
            count = 2 if self.mode=='spread' else int(seeding)
            for ref in ('own_countered','foe_countered'):
                self.counters(ref,count,negative=True)
        self.counters('clean_bear',0,negative=True)
        self.receipt()

    def meadowboon(self):
        evoked = self.mode.startswith('evoke')
        options = dict(printed='{3}{W}') if evoked else dict(mode=0)
        if self.mode=='evoke_countered':
            self.countered(**options)
            for seat in (self.owner,self.foe):
                self.meadow_team(seat,0)
            return
        self.announce('source',**options)
        target = self.foe if self.mode=='target_foe' else self.owner
        if evoked:
            self.emit('choose_player',player=self.owner,value=target)
        self.resolve()
        self.move('source','graveyard' if evoked else 'battlefield')
        if not evoked:
            self.stats('source')
            for seat in (self.owner,self.foe):
                self.meadow_team(seat,0)
            self.receipt()
            self.exit_source(target_player=target,power=3)
        for seat in (self.owner,self.foe):
            self.meadow_team(seat,int(seat==target))
        self.receipt()

    def meadow_team(self,seat,count):
        for kind in ('bear','spider'):
            self.counters(f'{kind}_{seat}',count)
        self.emit('assert_counter',f'artifact_{seat}',prop='plus1',value=0)

    def servant(self):
        if self.mode=='countered':
            self.countered()
            return
        self.announce('source')
        self.resolve()
        self.move('source','battlefield')
        amount = 1
        self.counters('source',amount)
        if 'change' in self.cards:
            self.announce('change',target='source')
            self.resolve()
            self.move('change','graveyard')
            amount += int(self.mode=='larger')
            self.emit('assert_counter','source',prop='plus1',value=amount)
            self.stats('source',amount+(3 if self.mode=='buffed' else 0))
        legal = self.mode not in ('no_recipient','bounce')
        self.exit_source(target='target' if legal else None)
        if 'target' in self.cards:
            self.counters('target',amount if legal else 0)
            self.counters('other_ally',0)
        self.counters('opposing',0)
        self.receipt()

    def follower_setup(self):
        mode,p,q = self.mode,self.owner,self.foe
        if self.id == WITCH:
            self.add('guard',SPIDER,q)
            self.load_deck(p, ([CURSE] if mode in ('search','decline','combat') else [])+['Forest'])
            self.load_deck(q,['Island','Mountain','Swamp'])
        elif self.id == MANTLE:
            self.add('host',BEAR,p)
            if mode=='host_dies':
                self.add('host_exit',MURDER,q,'hand')
        elif self.id == AURAMANCERS:
            self.load_deck(p, ([ENCHANTMENT] if mode!='whiff' else [])+['Forest'])
        elif self.id == NECROPEDE:
            self.add('recipient',BEAR,p)
            self.add('opposing',SPIDER,q)
        elif self.id == INCARNATION:
            if mode=='odd':
                self.add('burn',BOLT,q,'hand')
        elif self.id == SEER:
            self.load_deck(p,['Island','Plains','Forest'])
            if mode in ('shadow','shadow_block'):
                self.add('ground',BEAR,q)
            if mode=='shadow_block':
                self.add('shadow',SHADOW,q)
        elif self.id == VASTWOOD:
            self.add('first',BEAR,p)
            self.add('second',SPIDER,p)
            self.add('opposing',BEAR,q)
        elif self.id == VODALIAN:
            self.add('foe_folk',MERFOLK,q)
            self.add('nonfolk',BEAR,p)
            if mode!='foreign_only':
                self.add('folk',MERFOLK,p)
            if mode=='pump_twice':
                self.add('other_folk',MERFOLK,p)
        elif self.id == SKYCAPTAIN:
            self.add('weak',BEAR,p)
            self.add('strong',SPIDER if mode!='tie' else BEAR,p)
            self.add('enemy',SMALL,q)
        elif self.id == RESHAPER:
            if mode!='empty':
                top = {'accept':BEAR,'decline':BEAR,'large':SPIDER,'spell':GROWTH,'land':'Forest'}[mode]
                self.load_deck(p,[top,'Island','Swamp'])
        elif self.id == IMPLEMENT:
            self.load_deck(p,['Island','Forest','Mountain'])
            if mode=='ability':
                self.add('discarded','Swamp',q,'hand')
        elif self.id == KEEPER:
            opening = {'cast':['Forest',BEAR,'Island'],'decline':[BEAR,'Island'],
                       'whiff':['Forest'],'large':[SPIDER,BEAR,'Island']}
            self.load_deck(p,opening[mode])
        elif self.id == SWARM:
            for ref,seat in (('first',p),('second',q),('third',q)):
                self.add(ref,WALL,seat)
        if mode in ('shadow','shadow_block','reassign','foreign_only','proactive','poor_mana'):
            return
        if self.id == WITCH and mode=='combat' or self.id == AURAMANCERS and mode in ('search','decline','whiff'):
            return
        if self.id == MANTLE and mode in ('pump','host_dies') or self.id == NECROPEDE and mode in ('infect_player','infect_block'):
            return
        if self.id == IMPLEMENT and mode=='ability':
            return
        removal = self.follower_removal()
        if removal:
            self.add('exit',removal,q,'hand')

    def follower_removal(self):
        if self.mode=='bounce':
            return BOUNCE_PERMANENT if self.id in (MANTLE,IMPLEMENT) else BOUNCE
        if self.mode=='exile':
            return EXILE
        return {MANTLE:NATURALIZE, IMPLEMENT:SHATTER, KEEPER:SHATTER}.get(self.id,MURDER)

    def load_deck(self,seat,items):
        for name in items:
            self.add(f'deck_{seat}_{len(self.deck[seat])}',name,seat,'library')

    def follower_enter(self,*,target=None,x=None):
        self.announce('source',target=target,x=x)
        self.resolve()
        self.move('source','battlefield')

    def follower_exit(self,*,target='source',choices=None,destination='graveyard'):
        self.announce('exit',target=target)
        if choices:
            choices()
        self.resolve()
        self.move(target,destination)
        self.move('exit','graveyard')

    def queued_zone(self,name):
        self.emit('choose_zone_card',player=self.owner,value=self.rows[name]['csvid'])

    def optional_trigger(self):
        self.emit('choose_trigger', 'source',player=self.owner)

    def take_top(self,zone):
        self.move(self.deck[self.owner].pop(0),zone)

    def one_seat_upkeep(self,seat,choices=None):
        self.emit('advance_turn')
        self.emit('untap_step',player=seat)
        if choices:
            choices()
        self.emit('upkeep_step',player=seat)
        self.choices()
        self.emit('turn',player=seat)

    def witch(self):
        def accept():
            if self.owner==0:
                self.optional_trigger()
            self.queued_zone(CURSE)
            self.emit('choose_player',player=self.owner,value=self.foe)

        mode = self.mode
        self.follower_enter()
        if mode=='combat':
            self.start_combat('guard')
            accept()
            self.finish_combat()
            self.move('source','graveyard')
            self.move('guard','graveyard')
        else:
            def whiff():
                if self.owner==0:
                    self.optional_trigger()
                self.emit('choose_zone',player=self.owner,value=-1)
            choice = {'search':accept,'whiff':whiff}.get(mode)
            self.follower_exit(choices=choice,destination='hand' if mode=='bounce' else 'graveyard')
        if mode in ('combat','search'):
            self.take_top('battlefield')
            self.emit('assert_shuffle_count',player=self.owner,value=1)
            self.one_seat_upkeep(self.foe)
            for _ in range(2):
                self.move(self.deck[self.foe].pop(0),'graveyard')
        else:
            self.emit('assert_shuffle_count',player=self.owner,value=int(mode=='whiff'))
        self.receipt()

    def mantle(self):
        self.follower_enter(target='host')
        if self.mode=='pump':
            self.pool(self.owner,[0,0,0,0,1,0],fund=True)
            self.emit('can_activate','source',value=1)
            self.emit('activate','source',value=1)
            self.emit('assert_stack',value=1)
            self.wallet[self.owner] = ZERO.copy()
            self.resolve()
            self.observe('host','power',3)
            self.observe('host','toughness',2)
            self.receipt()
            self.visit(self.foe)
            self.stats('host')
        elif self.mode=='host_dies':
            self.announce('host_exit',target='host')
            self.resolve()
            self.move('host_exit','graveyard')
            self.move('host','graveyard')
            self.move('source','hand')
        else:
            self.follower_exit(destination='hand')
        self.receipt()

    def auramancers(self):
        # Seat 1 legally passes on this optional death trigger in the calibrated fixture.
        accepts = self.owner==0 and self.mode in ('search','whiff')
        def search():
            if accepts:
                self.optional_trigger()
                if self.mode=='search':
                    self.queued_zone(ENCHANTMENT)
                else:
                    self.emit('choose_zone',player=self.owner,value=-1)

        self.follower_enter()
        self.emit('assert_counter','source',prop='time',value=3)
        self.one_seat_upkeep(self.foe)
        self.emit('assert_counter','source',prop='time',value=3)
        if self.mode=='early':
            self.follower_exit()
            self.emit('assert_shuffle_count',player=self.owner,value=0)
        else:
            for count in (2,1,0):
                self.one_seat_upkeep(self.owner,search if count==0 else None)
                if count:
                    self.emit('assert_counter','source',prop='time',value=count)
                    self.one_seat_upkeep(self.foe)
            self.move('source','graveyard')
            if accepts and self.mode=='search':
                self.take_top('battlefield')
            self.emit('assert_shuffle_count',player=self.owner,value=int(accepts))
        self.receipt()

    def start_combat(self,blocker=None,*,illegal=None):
        self.visit(self.foe)
        self.visit(self.owner)
        self.emit('begin_attackers')
        self.emit('attack','source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if illegal:
            self.emit('block',illegal,'source',value=0)
        if blocker:
            self.emit('block',blocker,'source',value=1)
        self.emit('advance_combat')
        self.observe('source','blocked',int(bool(blocker)))

    def finish_combat(self):
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat_step')
        self.resolve()

    def necropede(self):
        def choose():
            op = 'choose_optional_target' if self.mode=='decline' else 'choose_target'
            self.emit(op,'recipient',player=self.owner)

        mode = self.mode
        self.follower_enter()
        if mode in ('infect_player','infect_block'):
            self.start_combat('opposing' if mode=='infect_block' else None)
            if mode=='infect_block':
                choose()
            self.finish_combat()
            if mode=='infect_block':
                self.move('source','graveyard')
                self.counters('opposing',1,negative=True)
                self.observe('opposing','damage',0)
        else:
            self.follower_exit(choices=choose if mode in ('dies','decline') else None,
                               destination='hand' if mode=='bounce' else 'graveyard')
            if mode=='decline':
                self.emit('assert_forced_target_choices',value=0)
        self.counters('recipient',int(mode in ('infect_block','dies')),negative=True)
        for seat in (0,1):
            self.emit('assert_poison',player=seat,value=int(seat==self.foe and mode=='infect_player'))
        self.receipt()

    def incarnation(self):
        self.follower_enter()
        self.stats('source')
        if self.mode=='proactive':
            # The printed zero-cost ability is available before a damage packet exists.
            self.emit('can_activate','source',value=1)
            self.receipt()
            return
        if self.mode=='odd':
            self.pool(self.foe,cost_from_oracle(self.oracle[BOLT]),fund=True)
            self.emit('choose_player',player=self.foe,value=self.owner)
            self.emit('cast','burn',value=1)
            self.wallet[self.foe] = ZERO.copy()
            self.resolve()
            self.move('burn','graveyard')
            self.life[self.owner] -= 3
            self.receipt()
        self.follower_exit(destination='hand' if self.mode=='bounce' else 'graveyard')
        if self.mode!='bounce':
            self.life[self.owner] //= 2
        self.receipt()

    def su_chi(self):
        self.follower_enter()
        self.stats('source')
        self.receipt()
        zones = {'bounce':'hand','exile':'exile'}
        self.exit_source(power=4)
        if self.mode not in zones:
            self.wallet[self.owner][0] = 4
        self.receipt()

    def seer(self):
        self.follower_enter()
        if self.mode in ('shadow','shadow_block'):
            self.start_combat('shadow' if self.mode=='shadow_block' else None,illegal='ground')
            self.finish_combat()
            if self.mode=='shadow_block':
                self.move('source','graveyard')
                self.take_top('hand')
            else:
                self.life[self.foe] -= 1
        else:
            self.exit_source(power=1)
            self.take_top('hand')
        self.receipt()

    def vastwood(self):
        self.follower_enter(x=3)
        self.counters('source',3)
        plan = {'split':('first','second','second'),'single':('first',)*3}
        targets = plan.get(self.mode,())
        def choose():
            for ref in targets:
                self.emit('choose_target',ref,player=self.owner)
            if self.mode=='decline':
                self.emit('choose_cancel_target',player=self.owner)
        self.follower_exit(choices=choose if self.mode!='bounce' else None,
                           destination='hand' if self.mode=='bounce' else 'graveyard')
        for ref in ('first','second','opposing'):
            self.counters(ref,targets.count(ref))
        self.receipt()

    def vodalian(self):
        def activate(ref,mode):
            self.emit('can_activate','source',value=1)
            self.emit('choose_dialog',player=self.owner,value=mode)
            self.emit('choose_target',ref,player=self.owner)
            self.emit('activate','source',value=1)
            self.emit('assert_stack',value=1)
            self.observe(ref,'tapped',1)
            self.resolve()

        self.follower_enter()
        self.stats('source')
        if self.mode=='foreign_only':
            self.emit('can_activate','source',value=0)
            self.receipt()
            return
        if self.mode=='reassign':
            self.visit(self.foe)
            self.visit(self.owner)
            self.emit('begin_attackers')
            self.emit('attack','source',value=0)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.finish_combat()
            self.visit(self.foe)
            self.visit(self.owner)
            activate('folk',0)
            self.emit('begin_attackers')
            self.emit('attack','source',value=1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.finish_combat()
            self.visit(self.foe)
            self.visit(self.owner)
            self.emit('begin_attackers')
            self.emit('attack','source',value=0)
            self.emit('finish_attackers')
            self.receipt()
            return
        paid = ('folk','other_folk') if self.mode=='pump_twice' else ('folk',)
        for n,ref in enumerate(paid,1):
            activate(ref,1)
            self.observe('source','power',2*n)
            self.observe('source','toughness',4+n)
        if self.mode=='expiry':
            self.visit(self.foe)
            self.stats('source')
        self.follower_exit()
        if self.mode!='expiry':
            for ref in paid:
                self.move(ref,'graveyard')
        self.receipt()

    def skycaptain(self):
        self.follower_enter()
        if self.mode=='flying':
            self.start_combat(illegal='enemy')
            self.finish_combat()
            self.life[self.foe] -= 2
            self.receipt()
            return
        choice = lambda: self.emit('choose_target','weak',player=self.owner)
        self.follower_exit(choices=choice if self.mode=='tie' else None,
                           destination='hand' if self.mode=='bounce' else 'graveyard')
        self.counters('weak',0 if self.mode=='bounce' else 2)
        self.counters('strong',0)
        self.counters('enemy',0)
        self.receipt()

    def reshaper(self):
        self.follower_enter()
        mode = self.mode
        def choose():
            placement = mode in ('accept','decline','land')
            if placement or (self.owner==0 and mode in ('large','spell')):
                self.emit('choose_dialog',player=self.owner,value=0)
            if placement:
                self.emit('choose_dialog',player=self.owner,value=int(mode=='decline'))
        self.follower_exit(choices=choose)
        if mode!='empty':
            self.take_top('battlefield' if mode in ('accept','land') else 'hand')
        self.receipt()

    def implement(self):
        self.follower_enter()
        if self.mode=='ability':
            self.emit('discard_policy',value=1)
            self.pool(self.owner,[0,1,0,0,0,0],fund=True)
            self.emit('can_activate','source',value=1)
            self.emit('choose_player',player=self.owner,value=self.foe)
            self.emit('activate','source',value=1)
            self.wallet[self.owner] = ZERO.copy()
            self.move('source','graveyard')
            self.take_top('hand')
            self.emit('assert_zone',player=self.owner,zone=ZONES['hand'],value=1)
            self.emit('assert_zone',player=self.foe,zone=ZONES['hand'],value=1)
            self.resolve()
            self.move('discarded','graveyard')
        else:
            self.follower_exit(destination='hand' if self.mode=='bounce' else 'graveyard')
            if self.mode!='bounce':
                self.take_top('hand')
        self.receipt()

    def keeper(self):
        self.follower_enter()
        def revealed():
            self.emit('choose_zone',player=self.owner,value=-1)
            self.emit('choose_zone',player=self.foe,value=-1)
            if self.mode=='decline':
                self.emit('choose_dialog',player=self.owner,value=1)
        self.follower_exit(choices=revealed)
        deck = self.deck[self.owner]
        if self.mode in ('cast','large'):
            prefix = deck.pop(0)
            self.take_top('battlefield')
            deck.append(prefix)
        elif self.mode=='decline':
            deck.append(deck.pop(0))
        self.receipt()

    def swarm(self):
        self.follower_enter()
        plans = {'split':('first','second','second'),'three':('first','second','third'),'single':('first',)*3}
        targets = plans.get(self.mode,())
        def allocate():
            for ref in targets:
                self.emit('choose_target',ref,player=self.owner)
        self.follower_exit(choices=allocate if targets else None,
                           destination='hand' if self.mode=='bounce' else 'graveyard')
        for ref in ('first','second','third'):
            self.counters(ref,targets.count(ref),negative=True)
        self.visit(self.foe)
        for ref in ('first','second','third'):
            self.counters(ref,0,negative=True)
        self.receipt()

    def run(self):
        self.emit('turn',player=self.owner)
        self.emit('present_ai_dialogs',value=1)
        for seat in (0,1):
            self.pool(seat,ZERO,fund=True)
        self.receipt()
        methods = {HYDRA:self.hydra,SNAPPER:self.snapper,MEADOW:self.meadowboon,SERVANT:self.servant,
                   WITCH:self.witch,MANTLE:self.mantle,AURAMANCERS:self.auramancers,NECROPEDE:self.necropede,
                   INCARNATION:self.incarnation,SU_CHI:self.su_chi,SEER:self.seer,VASTWOOD:self.vastwood,
                   VODALIAN:self.vodalian,SKYCAPTAIN:self.skycaptain,RESHAPER:self.reshaper,
                   IMPLEMENT:self.implement,KEEPER:self.keeper,SWARM:self.swarm}
        methods[self.id]()
