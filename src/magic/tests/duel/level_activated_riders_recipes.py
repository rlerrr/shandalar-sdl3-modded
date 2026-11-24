"""Paid Level Up thresholds and their real activated abilities."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counted_battlefield_damage_recipes import types
from death_counter_trigger_recipes import exact
from level_up_case_support import LevelUpBoard, classify_leveler


FAMILY = 'level_activated_riders'
PROFILE = Path(__file__).parent/'data/level_activated_riders_wave8_profiles.json'
RULES = {
    (('{T}: This creature deals 1 damage to any target.',),
     ('{T}: This creature deals 3 damage to any target.',)): ('damage', ''),
    (('{U}{U}, {T}: Copy target instant or sorcery spell. You may choose new targets for the copy.',),
     ('{U}{U}, {T}: Copy target instant or sorcery spell twice. You may choose new targets for the copies.',)): ('copy', '{U}{U}'),
    (('{T}: Draw a card, then discard a card.',), ('{T}: Draw a card.',)): ('loot', ''),
    (('{B}, {T}: Target creature gets -2/-2 until end of turn.',),
     ('{B}, {T}: Target creature gets -4/-4 until end of turn.',)): ('shrink', '{B}'),
    (('{T}: Create a 3/3 green Elephant creature token.',),
     ('{T}: Create two 3/3 green Elephant creature tokens.',)): ('elephant', ''),
    ((), ('{B}: Regenerate this creature.',)): ('regenerate', '{B}'),
}
MODES = {
    'damage': ('below','first','last_lower','upper','fresh','walker','creature','replay'),
    'copy': ('below','first','upper','short','no_parent','wrong_parent','retarget','replay'),
    'loot': ('below','first','last_lower','upper','fresh','empty_library','timing','replay'),
    'shrink': ('below','first','upper','short','expiry','lethal','fresh','replay'),
    'elephant': ('below','first','last_lower','upper','fresh','timing','replay'),
    'regenerate': ('below','first_band','upper','short','wrong_color','exile','timing','replay'),
}
DECK = ('Forest','Island','Mountain','Plains')
ELEPHANT = 'Elephant'
VANILLA = {'Grizzly Bears','Hill Giant','Enormous Baloth'}
WITNESSES = set(DECK) | VANILLA | {
    'Jace Beleren','Reach Through Mists','Lightning Bolt',ELEPHANT,'Murder','Swords to Plowshares',
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    shape = classify_leveler(record)
    key = tuple(tuple(shape[band]) for band in ('first_riders','second_riders'))
    role = RULES.get(key)
    if not role:
        raise ValueError('complete activated level-band Oracle required')
    effect, fee = role
    keywords = ('Level Up','Regenerate') if effect=='regenerate' else ('Level Up',)
    if sorted(record['keywords']) != sorted(keywords):
        raise ValueError('unexpected printed keyword list')
    return dict(shape,effect=effect,rider_mana=cost_from_oracle({'mana_cost':fee}) if fee else [0]*6)


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0,1) for mode in MODES[shape['effect']]]


def spec(declaration,record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text']
            or not exact(profiles()['sources'].get(record['name'],{}).get('oracle'),record)
            or not exact(declaration.get('params'),shape)):
        raise ValueError('exact printed leveler and both activation bands required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct reviewed leveled activation variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if set(pins['witnesses']) != WITNESSES or len(pins['sources']) != len(RULES):
        raise ValueError('complete level source and witness scope required')
    for name,pin in (pins['sources'] | pins['witnesses']).items():
        row = rows[name]
        if (not exact(oracle[name],pin['oracle']) or not exact(row,pin['registry'])
                or row['kind'] not in (('card','token') if name==ELEPHANT else ('card',))
                or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in VANILLA):
            raise ValueError(f'pinned leveler or independent witness changed: {name}')
    if rows[ELEPHANT]['csvid'] != 18729 or oracle[ELEPHANT]['type_line'] != 'Token Creature — Elephant':
        raise ValueError('native Elephant binding changed')


class ActivatedLeveler(LevelUpBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.effect = shape['effect']
        self.life = [20,20]
        self.walker = None
        self.discard_choices = 0
        self.replay = None
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode = self.mode
        if self.effect=='damage' and mode in ('walker','creature'):
            name = 'Jace Beleren' if mode=='walker' else 'Grizzly Bears'
            self.add(name,'victim',self.foe)
            if mode=='walker':
                self.walker = int(self.oracle[name]['loyalty'])
        if self.effect=='shrink':
            self.add('Grizzly Bears' if mode=='lethal' else 'Enormous Baloth','victim',self.foe)
        if self.effect=='copy' and mode!='no_parent':
            name = {'wrong_parent':'Grizzly Bears','retarget':'Lightning Bolt'}.get(mode,'Reach Through Mists')
            self.add(name,'parent',self.foe,'hand')
        if self.effect=='regenerate' and mode!='timing':
            name = 'Swords to Plowshares' if mode=='exile' else 'Murder'
            self.add(name,'removal',self.foe,'hand')
        if self.effect in ('loot','copy'):
            for who in (0,1):
                if mode=='empty_library' and who==self.owner:
                    # A one-card library is consumed by the real activation.
                    size = 1
                else:
                    size = len(DECK)
                for index,name in enumerate(DECK[:size]):
                    self.add(name,f'library_{who}_{index}',who,'library')

    def stat(self, ref, prop, value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def spend(self, who, fee):
        result = [have-due for have,due in zip(self.pools[who],fee)]
        assert min(result)>=0
        self.pools[who] = result

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        quantities = Counter((card['player'],card['zone'],self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone in ('battlefield','hand','graveyard','exile','library'):
                self.emit('assert_zone',player=who,zone=zone,
                          value=sum(count for (p,z,_),count in quantities.items() if p==who and z==zone))
            ordered = [card['name'] for card in self.cards.values() if card['player']==who and card['zone']=='library']
            for index,name in enumerate(ordered):
                self.emit('assert_zone_card',player=who,zone='library',index=index,value=self.rows[name]['csvid'])
        for (who,zone,csvid),value in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity',player=who,zone=zone,index=csvid,value=value)
        for ref,card in self.cards.items():
            if card['zone']!='battlefield' or card['name']==ELEPHANT:
                continue
            self.stat(ref,'in_play',1)
            self.stat(ref,'tapped',card['tapped'])
            if ref=='source':
                self.observe_level()
            elif 'Creature' in types(self.oracle[card['name']]):
                record = self.oracle[card['name']]
                for field in ('power','toughness'):
                    self.stat(ref,field,int(record[field])+card['boost'])
                self.stat(ref,'damage',card['damage'])
        if self.walker is not None and self.cards['victim']['zone']=='battlefield':
            self.emit('assert_counter',ref='victim',counter_kind='loyalty',value=self.walker)
        if self.effect=='loot':
            self.emit('assert_discard_choices',value=self.discard_choices)
        if any(card['name']==ELEPHANT for card in self.cards.values()):
            for prop,value in (('token',1),('subtype_Elephant',1),('color',8),('power',3),('toughness',3)):
                self.emit('assert_matching_permanents',player=self.owner,index=self.rows[ELEPHANT]['csvid'],
                          property=prop,value=value)

    def target_level(self):
        first, upper = self.shape['first'][0],self.shape['second'][0]
        if self.mode in ('below','timing'):
            return upper-1 if self.effect=='regenerate' else first-1
        if self.mode in ('first','first_band','retarget','empty_library','lethal'):
            return first
        if self.mode=='last_lower':
            return upper-1
        return upper

    def choose_level(self,board):
        assert board is self
        if self.effect!='regenerate' and (self.owner==1 or self.effect=='damage' and self.level>=1):
            self.emit('choose_dialog',player=self.owner,value=0)

    def choose_rider(self):
        if self.owner==1 or self.effect=='damage':
            self.emit('choose_dialog',player=self.owner,value=1)

    def start_ordinary_rider(self, allowed):
        fee = self.shape['rider_mana']
        budget = [0]*6 if self.mode=='short' else fee
        self.fund(self.owner,budget)
        self.emit('can_activate',ref='source',value=int(allowed))
        if allowed:
            self.choose_rider()
            if self.effect=='damage':
                if self.mode in ('walker','creature'):
                    self.emit('choose_target',player=self.owner,ref='victim')
                else:
                    self.emit('choose_player',player=self.owner,value=self.foe)
            elif self.effect=='shrink':
                self.emit('choose_target',player=self.owner,ref='victim')
        self.emit('activate',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.spend(self.owner,fee)
            self.cards['source']['tapped'] = True
            self.stat('source','tapped',1)
        self.observe_mana()

    def damage_effect(self):
        amount = 3 if self.level>=self.shape['second'][0] else 1
        if self.mode=='walker':
            self.walker -= amount
            if self.walker==0:
                self.cards['victim']['zone'] = 'graveyard'
        elif self.mode=='creature':
            self.cards['victim']['damage'] = amount
            if amount>=int(self.oracle[self.cards['victim']['name']]['toughness']):
                self.cards['victim']['zone'] = 'graveyard'
        else:
            self.life[self.foe] -= amount

    def loot_effect(self):
        self.draw(self.owner)
        if self.level<self.shape['second'][0]:
            drawn_ref, = [ref for ref,card in self.cards.items() if card['player']==self.owner and card['zone']=='hand']
            self.cards[drawn_ref]['zone'] = 'graveyard'
            self.discard_choices += int(self.owner==0)

    def tokens_effect(self):
        quantity = 2 if self.level>=self.shape['second'][0] else 1
        for index in range(quantity):
            self.cards[f'elephant_{index}'] = dict(name=ELEPHANT,player=self.owner,zone='battlefield',tapped=False,damage=0,boost=0)

    def shrink_effect(self):
        card = self.cards['victim']
        card['boost'] = -4 if self.level>=self.shape['second'][0] else -2
        if int(self.oracle[card['name']]['toughness'])+card['boost']<=0:
            card['zone'] = 'graveyard'

    def ordinary(self):
        allowed = self.mode not in ('below','fresh','short')
        if self.mode=='timing':
            self.advance_turn()
            self.fund(self.owner,self.shape['activation'])
            self.emit('can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        self.start_ordinary_rider(allowed)
        if allowed:
            self.emit('resolve')
            self.emit('assert_unused_choices',value=0)
            self.emit('refresh')
            {'damage':self.damage_effect,'loot':self.loot_effect,'elephant':self.tokens_effect,
             'shrink':self.shrink_effect}[self.effect]()
        self.receipts()
        if self.mode=='expiry':
            self.emit('cleanup')
            self.emit('refresh')
            self.cards['victim']['boost'] = 0
            self.receipts()

    def copy_rider(self):
        if self.mode=='no_parent':
            self.advance_turn()
            self.fund(self.owner,self.shape['rider_mana'])
            self.emit('can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        allowed = self.mode not in ('below','short','wrong_parent')
        is_bolt = self.mode=='retarget'
        is_creature = self.mode=='wrong_parent'
        is_draw = not is_bolt and not is_creature
        if is_creature:
            self.advance_turn()
        if is_bolt:
            self.fund(self.foe,[0,0,0,0,1,0])
            self.emit('choose_player',player=self.foe,value=self.owner)
            self.emit('cast',ref='parent',value=1)
            self.spend(self.foe,[0,0,0,0,1,0])
            self.cards['parent']['zone'] = 'stack'
            self.emit('assert_stack',value=1)
        else:
            self.cast('parent')
        self.emit('announce_spell',ref='parent')
        self.stat('parent','in_hand',0)
        self.fund(self.owner,[0,0,1,0,0,0] if self.mode=='short' else self.shape['rider_mana'])
        if allowed:
            self.choose_rider()
        self.emit('activate_response',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=1+int(allowed))
        copies = 0
        if allowed:
            self.spend(self.owner,self.shape['rider_mana'])
            self.cards['source']['tapped'] = True
            self.stat('source','tapped',1)
            copies = 2 if self.level>=self.shape['second'][0] else 1
        self.observe_mana()
        if is_bolt:
            self.emit('choose_player',player=self.owner,value=self.foe)
        self.emit('resolve_response' if allowed else 'resolve')
        self.emit('assert_unused_choices',value=0)
        self.emit('refresh')
        self.cards['parent']['zone'] = 'battlefield' if is_creature else 'graveyard'
        if is_draw:
            for _ in range(copies):
                self.draw(self.owner)
            self.draw(self.foe)
        if is_bolt:
            self.life[self.owner] -= 3
            self.life[self.foe] -= 3*copies
        self.receipts()

    def regeneration(self):
        if self.mode=='timing':
            self.advance_turn()
            self.fund(self.owner,self.shape['activation'])
            self.emit('can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        fee = self.shape['rider_mana']
        budget = [0]*6 if self.mode=='short' else [1,0,0,0,0,0] if self.mode=='wrong_color' else fee
        allowed = self.mode in ('upper','replay')
        self.fund(self.owner,budget)
        self.cast('removal',target='source')
        self.emit('choose_regeneration',ref='source')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=int(not allowed))
        self.emit('refresh')
        self.cards['removal']['zone'] = 'graveyard'
        if allowed:
            self.spend(self.owner,fee)
            self.cards['source']['tapped'] = True
        else:
            exile = self.mode=='exile'
            self.cards['source']['zone'] = 'exile' if exile else 'graveyard'
            if exile:
                self.life[self.owner] += self.source_stats()[0]
        self.receipts()

    def play(self):
        self.emit('present_ai_dialogs',value=1)
        if self.effect=='loot':
            self.emit('discard_policy',value=1)
        self.play_source()
        self.level_to(self.target_level(),choose=self.choose_level)
        self.receipts()
        if self.mode not in ('fresh','timing') and self.effect!='regenerate':
            self.advance_turn()
            self.advance_turn()
        if self.mode=='replay':
            self.replay = len(self.steps)
        if self.effect=='copy':
            self.copy_rider()
        elif self.effect=='regenerate':
            self.regeneration()
        else:
            self.ordinary()


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('unbound Level Up source or variant')
    gaps = ['Multiple multiplayer recipients, counter multiplication and stolen source ownership']
    if shape['effect']=='regenerate':
        gaps.append('Native regeneration callback only advertises the ability during LCBP_REGENERATION; proactive shield before destruction is not exercised.')
    if shape['effect']=='copy':
        gaps.append('Keeping the original target and independent changes of both copies are not yet calibrated.')
    meta = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
    cases = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board = ActivatedLeveler(name,shape,mode,int(seat),rows,oracle)
        board.play()
        steps = board.steps
        if board.replay is not None:
            at = board.replay
            tail = steps[at:]
            steps = [*steps[:at],step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                     step('restore'),*tail,step('assert_recorded_state')]
        cases.append(dict(name=f'LevelActivatedRiderRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2087,
                          setup=board.setup,steps=steps,_recipe=dict(meta,variant=variant)))
    return cases
