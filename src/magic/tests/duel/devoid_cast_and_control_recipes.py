"""Paid colorless spells and creature-control gates for Devoid permanents."""
from collections import Counter
import json
from pathlib import Path

from devoid_case_support import DevoidBoard, devoid_rules
from library_search_recipes import wave6_exact
from recipes import COLORS, cost_from_oracle


FAMILY = 'devoid_cast_and_control'
PROFILE = Path(__file__).parent / 'data/devoid_cast_and_control_wave8_profiles.json'
PUMP = 'Whenever you cast a colorless spell, this creature gets +1/+0 until end of turn.'
EXILE = 'Whenever you cast a colorless spell, target opponent exiles the top card of their library.'
REGEN = '{1}{B}: Regenerate this creature. Activate only if you control another colorless creature.'
COUNT = "Vile Aggregate's power is equal to the number of colorless creatures you control."
INGEST = 'Ingest (Whenever this creature deals combat damage to a player, that player exiles the top card of their library.)'
HANDLES = {
    ('{1}: Target colorless creature gains haste until end of turn.',): 'grant',
    (PUMP,): 'pump',
    ('Whenever you cast a colorless spell, this enchantment deals 1 damage to any target.',): 'damage',
    ('This creature has haste as long as you control another colorless creature.',): 'gated_haste',
    ('At the beginning of your upkeep, you lose 1 life unless you control another colorless creature.',): 'upkeep',
    ('Flying', PUMP): 'flying_pump',
    ('Flying', EXILE): 'flying_exile',
    ("This creature can't block.", REGEN): 'gated_regeneration',
    (COUNT,'Trample',INGEST): 'aggregate',
}
CASTS = ('own_devoid', 'own_artifact', 'own_colored', 'enemy_devoid', 'enemy_artifact',
         'own_repeat', 'countered', 'token_entry')
SUPPORT = ('alone', 'ally_devoid', 'ally_artifact', 'ally_colored', 'enemy_only', 'noncreature', 'lose_ally')
MODES = {
    'grant': ('grant_self', 'grant_devoid', 'grant_enemy', 'short_fee', 'target_probes'),
    'pump': (*CASTS, 'expire_pump'),
    'flying_pump': (*CASTS, 'expire_pump', 'flight_ground', 'flight_reach'),
    'damage': (*CASTS, 'target_creature', 'target_walker', 'target_self'),
    'gated_haste': (*SUPPORT, 'gain_after'),
    'upkeep': (*SUPPORT, 'late_gain'),
    'flying_exile': ('own_devoid', 'own_artifact', 'own_colored', 'enemy_devoid', 'own_repeat',
                     'empty_library', 'protected_opponent', 'flight_ground', 'flight_reach'),
    'gated_regeneration': ('regen_devoid','regen_artifact','alone','ally_colored','enemy_only',
                           'removed_support','short','cannot_block'),
    'aggregate': ('solo','mixed','lose_ally','trample_excess','blocked','empty_library','unblocked'),
}
DEVOID_ALLY = 'Culling Drone'
ARTIFACT_ALLY = 'Bronze Sable'
COLORED_ALLY = 'Grizzly Bears'
RELIC = "Feldon's Cane"
SMALL = "Mons's Goblin Raiders"
REACH = 'Wall of Vines'
DECK = ('Forest', 'Island', 'Plains', 'Swamp')
DUMMIES = {ARTIFACT_ALLY, COLORED_ALLY, SMALL, REACH, 'Myr'}
WITNESSES = {*DECK, *DUMMIES, DEVOID_ALLY, RELIC, 'Myr Sire', 'Lightning Bolt', 'Unsummon',
             'Jace Beleren', 'Counterspell', 'Murder', 'Leyline of Sanctity'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if not record:
        raise ValueError('complete Devoid Oracle required')
    lines = tuple(devoid_rules(record))
    if lines not in HANDLES:
        raise ValueError('unreviewed complete Devoid clauses')
    effect = HANDLES[lines]
    extra = {'flying_pump':('Flying',), 'flying_exile':('Flying',), 'gated_regeneration':('Regenerate',), 'aggregate':('Trample','Ingest')}
    if set(record['keywords']) != {'Devoid', *extra.get(effect,())}:
        raise ValueError('whole printed keyword set changed')
    stats = (record.get('power'), record.get('toughness'))
    if effect == 'damage':
        if record['type_line'] != 'Enchantment':
            raise ValueError('expected noncreature Devoid enchantment')
        power = toughness = None
    elif effect=='aggregate':
        if 'Creature' not in record['type_line'] or stats[0]!='*' or not stats[1].isdecimal():
            raise ValueError('expected printed variable power and fixed toughness')
        power,toughness=None,int(stats[1])
    else:
        if 'Creature' not in record['type_line'] or any(not isinstance(n,str) or not n.isdecimal() for n in stats):
            raise ValueError('expected fixed printed creature characteristics')
        power, toughness = map(int, stats)
    mana = {'grant':'{1}','gated_regeneration':'{1}{B}'}.get(effect)
    return dict(effect=effect, price=cost_from_oracle(record), power=power, toughness=toughness,
                fee=cost_from_oracle({'mana_cost':mana}) if mana else None)


def variants(shape):
    return [f'{mode}_{seat}' for mode in MODES[shape['effect']] for seat in (0, 1)]


def spec(declaration, record):
    name = record.get('name') if record else None
    source = profiles()['sources'].get(name)
    if not source or not wave6_exact(source['oracle'], record):
        raise ValueError('exact pinned whole Devoid record required')
    shape = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not wave6_exact(declaration.get('params'), shape) or not isinstance(chosen,list)
            or not chosen or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(shape))):
        raise ValueError('complete pinned Devoid declaration and distinct reviewed cases required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources','witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('incomplete independent Devoid witnesses')
    for group, pins in data.items():
        for name, pin in pins.items():
            native, record = rows[name], oracle[name]
            if (pin['registry'] != native or not wave6_exact(pin['oracle'],record) or native['kind'] != 'card'
                    or native['handler'] in (None,'0','NULL') or native['handler'] == 'card_dummy' and name not in DUMMIES):
                raise ValueError('unreviewed witness ' + name)
            if group == 'sources':
                classify(record)


class Board(DevoidBoard):
    def build(self):
        self.effect = self.shape['effect']
        self.life = [20,20]
        self.active = self.owner
        self.pump = 0
        for side in (0,1):
            if side==self.foe and self.mode=='empty_library':
                continue
            for index, name in enumerate(DECK):
                self.add(name, f'library_{side}_{index}', side, 'library')
        if self.effect in ('pump','flying_pump','damage','flying_exile'):
            self.build_casts()
        elif self.effect=='gated_regeneration':
            self.build_regeneration()
        elif self.effect=='aggregate':
            self.build_aggregate()
        elif self.effect in ('upkeep','gated_haste'):
            if self.mode not in ('alone','enemy_only'):
                name = {'ally_artifact':ARTIFACT_ALLY,'ally_colored':COLORED_ALLY,'noncreature':RELIC}.get(self.mode,DEVOID_ALLY)
                self.add(name,'support',self.owner,'hand')
            if self.mode == 'enemy_only':
                self.add(DEVOID_ALLY,'foreign',self.foe)
            if self.mode == 'lose_ally':
                self.add('Unsummon','bounce',self.foe,'hand')
        elif self.mode == 'target_probes':
            self.add(DEVOID_ALLY,'support',self.owner,'hand')
            self.add(ARTIFACT_ALLY,'foreign',self.foe)
            self.add(COLORED_ALLY,'colored',self.foe)
            self.add(RELIC,'relic',self.owner)
        elif self.mode in ('grant_devoid','grant_enemy'):
            self.add(DEVOID_ALLY,'support',self.foe if self.mode=='grant_enemy' else self.owner,'hand')

    def build_regeneration(self):
        if self.mode in ('regen_devoid','regen_artifact','ally_colored','removed_support','short'):
            name = {'regen_artifact':ARTIFACT_ALLY,'ally_colored':COLORED_ALLY}.get(self.mode,DEVOID_ALLY)
            self.add(name,'support',self.owner,'hand')
        if self.mode in ('enemy_only','cannot_block'):
            self.add(COLORED_ALLY if self.mode=='cannot_block' else DEVOID_ALLY,'foreign',self.foe)
        if self.mode=='removed_support':
            self.add('Unsummon','bounce',self.foe,'hand')
        if self.mode!='cannot_block':
            self.add('Murder','remove',self.foe,'hand')

    def build_aggregate(self):
        if self.mode in ('mixed','lose_ally','trample_excess','empty_library','unblocked'):
            self.add(DEVOID_ALLY,'support',self.owner,'hand')
        if self.mode=='mixed':
            self.add(ARTIFACT_ALLY,'artifact_ally',self.owner,'hand')
            self.add(COLORED_ALLY,'colored',self.owner)
            self.add(RELIC,'relic',self.owner)
            self.add(DEVOID_ALLY,'foreign',self.foe)
        if self.mode=='lose_ally':
            self.add('Unsummon','bounce',self.foe,'hand')
        if self.mode in ('trample_excess','blocked'):
            self.add(SMALL if self.mode=='trample_excess' else COLORED_ALLY,'blocker',self.foe)

    def build_casts(self):
        if self.mode=='protected_opponent':
            self.add('Leyline of Sanctity','leyline',self.foe)
        if self.mode == 'token_entry':
            self.add('Myr Sire','victim',self.owner)
            self.add('Lightning Bolt','bolt',self.owner,'hand')
        else:
            who = self.foe if self.mode.startswith('enemy_') else self.owner
            name = {'own_artifact':ARTIFACT_ALLY,'enemy_artifact':ARTIFACT_ALLY,
                    'own_colored':COLORED_ALLY}.get(self.mode,DEVOID_ALLY)
            self.add(name,'spell',who,'hand')
            if self.mode == 'own_repeat':
                self.add(ARTIFACT_ALLY,'again',self.owner,'hand')
        if self.mode == 'countered':
            self.add('Counterspell','counter',self.foe,'hand')
        if self.mode == 'target_creature':
            self.add(SMALL,'blocker',self.foe)
        if self.mode in ('flight_ground','flight_reach'):
            self.add(REACH if self.mode=='flight_reach' else COLORED_ALLY,'blocker',self.foe)
        if self.mode == 'target_walker':
            self.add('Jace Beleren','walker',self.foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])

    def creature(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def stats(self,ref):
        record = self.oracle[self.cards[ref]['name']]
        if ref=='source' and self.effect=='aggregate':
            power = sum(self.is_friendly_colorless(card) for card in self.cards.values())
            return power, int(record['toughness'])
        return int(record['power']) + (self.pump if ref=='source' else 0), int(record['toughness'])

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        for side in (0,1):
            self.emit('assert_life',player=side,value=self.life[side])
            for zone in ('hand','battlefield','graveyard','library','exile'):
                cards = [self.rows[item['name']]['csvid'] for item in self.cards.values()
                         if item['player']==side and item['zone']==zone]
                self.emit('assert_zone',player=side,zone=zone,value=len(cards))
                if zone=='library':
                    for index, csvid in enumerate(cards):
                        self.emit('assert_zone_card',player=side,zone=zone,index=index,value=csvid)
                else:
                    for csvid, count in sorted(Counter(cards).items()):
                        self.emit('assert_zone_multiplicity',player=side,zone=zone,index=csvid,value=count)
        seen = set()
        for ref, item in self.cards.items():
            if item['zone']!='battlefield':
                continue
            name = item['name']
            record = self.oracle[name]
            key = (item['player'],self.rows[name]['csvid'])
            if key not in seen:
                self.emit('assert_matching_permanents',player=key[0],index=key[1],property='color',
                          value=sum(1<<COLORS[color] for color in record['colors']))
                seen.add(key)
            if ref=='walker':
                self.emit('assert_counter',ref=ref,counter_kind='loyalty',value=self.loyalty)
            elif ref!='myr' and 'Creature' in record['type_line']:
                power,toughness = self.stats(ref)
                for prop,value in [('power',power),('toughness',toughness),('tapped',item['tapped']),('damage',item['damage'])]:
                    self.creature(ref,prop,value)

    def next_turn(self,*,upkeep=False):
        if any(map(any,self.pools)):
            raise ValueError('finish paying before real untap')
        self.emit('advance_turn')
        self.active = 1-self.active
        self.pump = 0
        for card in self.cards.values():
            card['damage'] = 0
            if card['zone']=='battlefield' and card['player']==self.active:
                card['tapped']=False
        self.emit('untap_step',player=self.active)
        if upkeep:
            self.emit('upkeep_step',player=self.active)
            if self.active==self.owner and not self.has_support():
                self.life[self.owner]-=1
        self.emit('turn',player=self.active,value=0)

    def is_friendly_colorless(self, card):
        record = self.oracle[card['name']]
        return (card['player']==self.owner and card['zone']=='battlefield'
                and 'Creature' in record['type_line'] and not record['colors'])

    def has_support(self):
        return any(ref!='source' and self.is_friendly_colorless(card) for ref,card in self.cards.items())

    def spell(self,ref,*,target=None,zone='graveyard'):
        self.cast(ref,target=target)
        self.emit('resolve')
        self.cards[ref]['zone']=zone

    def ingest(self,side):
        top = next((card for card in self.cards.values() if card['player']==side and card['zone']=='library'),None)
        if top:
            top['zone']='exile'

    def fight(self, ref, *, ready=True, block=None):
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=int(ready))
        self.cards[ref]['tapped'] = ready
        self.creature(ref,'attacking',ready)
        self.creature(ref,'tapped',ready)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if block is not None:
            self.emit('block',ref='blocker',other=ref,value=int(block))
        self.emit('advance_combat')
        self.creature(ref,'blocked',bool(block))
        self.emit('combat_damage')
        if ready:
            power,_ = self.stats(ref)
            if block:
                self.cards['blocker']['damage']=power
            else:
                self.life[1-self.active]-=power
                if self.cards[ref]['name']==DEVOID_ALLY:
                    self.ingest(1-self.active)
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def grant_haste(self):
        mode=self.mode
        if mode=='grant_enemy':
            self.next_turn()
        if 'support' in self.cards:
            self.spell('support',zone='battlefield')
        fee = self.shape['fee']
        if mode=='short_fee':
            self.emit('require_can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.fight('source',ready=False)
            return
        self.fund(self.owner,fee)
        self.emit('require_can_activate',ref='source',value=1)
        if mode=='target_probes':
            for ref, legal in [('source',1),('support',1),('foreign',1),('colored',0),('relic',0)]:
                self.emit('probe_activation_target',ref='source',other=ref,index=-1,value=legal)
            self.receipts()
            return
        target='source' if mode=='grant_self' else 'support'
        self.emit('choose_target',ref=target,player=self.owner)
        self.emit('activate',ref='source',value=1)
        self.emit('assert_stack',value=1)
        self.pools[self.owner]=[0]*6
        self.observe_mana()
        self.emit('resolve')
        self.fight(target)

    def damage_target(self):
        if self.mode in ('target_creature','target_walker'):
            self.emit('choose_target',ref='blocker' if self.mode=='target_creature' else 'walker',player=self.owner)
        else:
            self.emit('choose_player',player=self.owner,value=self.owner if self.mode=='target_self' else self.foe)

    def spell_trigger(self,ref):
        item=self.cards[ref]
        eligible=item['player']==self.owner and not self.oracle[item['name']]['colors']
        self.cast(ref)
        if eligible and self.effect=='damage':
            self.damage_target()
        self.emit('announce_spell',ref=ref)
        if self.mode=='countered':
            self.cast('counter',response=True)
            self.emit('resolve_response')
            self.cards['counter']['zone']='graveyard'
            item['zone']='graveyard'
        else:
            self.emit('resolve')
            item['zone']='battlefield'
        if eligible:
            if self.effect in ('pump','flying_pump'):
                self.pump += 1
            elif self.effect=='flying_exile':
                if self.mode!='protected_opponent':
                    self.ingest(self.foe)
            elif self.mode=='target_creature':
                self.cards['blocker']['zone']='graveyard'
            elif self.mode=='target_walker':
                self.loyalty-=1
            else:
                self.life[self.owner if self.mode=='target_self' else self.foe]-=1
        self.receipts()

    def cast_rider(self):
        mode=self.mode
        if mode=='token_entry':
            self.spell('bolt',target='victim')
            self.cards['victim']['zone']='graveyard'
            self.cards['myr']=dict(name='Myr',player=self.owner,zone='battlefield',tapped=False,damage=0)
            self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Myr']['csvid'],property='token',value=1)
            self.receipts()
            return
        if mode.startswith('enemy_') or mode.startswith('flight_'):
            self.next_turn()
        if mode.startswith('flight_'):
            self.next_turn()
        self.spell_trigger('spell')
        if mode=='own_repeat':
            self.spell_trigger('again')
        elif mode=='expire_pump':
            self.next_turn()
            self.receipts()
        elif mode.startswith('flight_'):
            self.fight('source',block=mode=='flight_reach')

    def control_gate(self):
        mode=self.mode
        if 'support' in self.cards and mode not in ('gain_after','late_gain'):
            self.spell('support',zone='battlefield')
            self.receipts()
        if self.effect=='upkeep':
            self.next_turn(upkeep=True)
            self.receipts()
        if mode=='lose_ally':
            self.spell('bounce',target='support')
            self.cards['support']['zone']='hand'
            self.receipts()
        if self.effect=='gated_haste':
            if mode=='gain_after':
                self.spell('support',zone='battlefield')
            self.fight('source',ready=self.has_support())
            return
        self.next_turn(upkeep=True)
        self.receipts()
        if mode=='late_gain':
            self.spell('support',zone='battlefield')
            self.next_turn(upkeep=True)
            self.next_turn(upkeep=True)
            self.receipts()

    def regenerate(self):
        mode=self.mode
        if mode=='cannot_block':
            self.next_turn()
            self.emit('begin_attackers')
            self.emit('attack',ref='foreign',value=1)
            self.cards['foreign']['tapped']=True
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('block',ref='source',other='foreign',value=0)
            self.emit('advance_combat')
            self.creature('foreign','blocked',0)
            self.emit('combat_damage')
            self.life[self.owner]-=2
            self.emit('resolve')
            self.emit('end_combat')
            self.receipts()
            return
        if 'support' in self.cards:
            self.spell('support',zone='battlefield')
        if mode=='removed_support':
            self.spell('bounce',target='support')
            self.cards['support']['zone']='hand'
        due=self.shape['fee']
        funds=[0]*6 if mode=='short' else due
        saved=self.has_support() and funds==due
        self.fund(self.owner,funds)
        self.cast('remove',target='source')
        self.emit('choose_regeneration',ref='source')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=int(not saved))
        self.cards['remove']['zone']='graveyard'
        if saved:
            self.pools[self.owner]=[0]*6
            self.cards['source']['tapped']=True
        else:
            self.cards['source']['zone']='graveyard'
        self.receipts()

    def aggregate(self):
        for ref in ('support','artifact_ally'):
            if ref in self.cards:
                self.spell(ref,zone='battlefield')
                self.receipts()
        if self.mode=='lose_ally':
            self.spell('bounce',target='support')
            self.cards['support']['zone']='hand'
            self.receipts()
        if self.mode in ('solo','mixed','lose_ally'):
            return
        self.next_turn()
        self.next_turn()
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.cards['source']['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        block='blocker' in self.cards
        if block:
            self.emit('block',ref='blocker',other='source',value=1)
        self.emit('advance_combat')
        self.creature('source','blocked',block)
        self.emit('combat_damage')
        power,_=self.stats('source')
        hit=power
        if block:
            block_power,block_toughness=self.stats('blocker')
            lethal=min(power,block_toughness)
            hit-=lethal
            self.cards['blocker']['damage']=lethal
            self.cards['source']['damage']=block_power
            if lethal==block_toughness:
                self.cards['blocker']['zone']='graveyard'
        if hit:
            self.life[self.foe]-=hit
            self.ingest(self.foe)
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def run(self):
        self.play_source()
        self.receipts()
        if self.effect=='grant':
            self.grant_haste()
        elif self.effect in ('pump','flying_pump','damage','flying_exile'):
            self.cast_rider()
        elif self.effect=='gated_regeneration':
            self.regenerate()
        elif self.effect=='aggregate':
            self.aggregate()
        else:
            self.control_gate()


def build(name, csvid, declaration, shape, variant, rows, oracle):
    mode, seat=variant.rsplit('_',1)
    world=Board(name,shape,mode,int(seat),rows,oracle)
    world.run()
    return dict(name=f'DevoidCastAndControlRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                seed=2172, setup=world.setup, steps=world.steps,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['In-place color changes and multiplayer.']))


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('distinct reviewed Devoid cases for the pinned native card required')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
