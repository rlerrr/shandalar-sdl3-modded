"""Actual creature-cast triggers with pinned effects, costs and contrast casts."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from cast_trigger_case_support import CastTriggerBoard
from counted_battlefield_damage_recipes import types
from death_counter_trigger_recipes import exact


FAMILY = 'cast_creature_riders'
PROFILE = Path(__file__).parent/'data/cast_creature_riders_wave8_profiles.json'
CAST = 'Whenever you cast a creature spell, '
RULES = {
    ('Protection from white and from black', CAST+'put a +1/+1 counter on Animar.',
     'Creature spells you cast cost {1} less to cast for each +1/+1 counter on Animar.'): ('counter',('Protection',),()),
    (CAST+'you may pay {1}. If you do, draw a card, then discard a card.',): ('loot',(),(1,0,0,0,0,0)),
    (CAST+"create X 1/1 black Thrull creature tokens, where X is that spell's mana value.",
     'When you control seven or more Thrulls, sacrifice Endrek Sahr.'): ('thrull',(),()),
    (CAST+"you may pay {1}. If you do, return target creature to its owner's hand.",): ('bounce',(),(1,0,0,0,0,0)),
    ('Indestructible',"As long as your devotion to green and white is less than seven, Karametra isn't a creature.",
     CAST+'you may search your library for a Forest or Plains card, put it onto the battlefield tapped, then shuffle.'):
        ('land',('Indestructible',),()),
}
COMMON = ('own','opponent','noncreature','countered','replay')
EXTRA = {
    'counter': ('reduction','colored_floor','protection'),
    'loot': ('unfunded','colored_fee'),
    'thrull': ('zero','below_threshold','threshold','opposing_thrulls'),
    'bounce': ('unfunded','own_target','foreign_owner','no_target'),
    'land': ('typed_land','plains','devotion_low','devotion_high','indestructible'),
}
THRULL = 'Thrull'
STATIC = {'Grizzly Bears','Hill Giant','Enormous Baloth','Ornithopter','Leatherback Baloth',"Garruk's Companion"}
WITNESSES = STATIC | {'Forest','Island','Plains','Savannah','Counterspell','Reach Through Mists','Elvish Mystic',
                      'Unsummon','Swords to Plowshares','Murder','Lightning Bolt','Vindicate',THRULL}
ZONES = ('battlefield','hand','graveyard','exile','library')


RULES.update({
    ('When this creature enters, draw a card.',
     CAST+'you may pay {G}{G}. If you do, return this card from your graveyard to your hand.'):
         ('recover',(),(0,0,0,2,0,0)),
    (CAST+'tap target creature.',): ('tap',(),()),
    (CAST+'you may put a quest counter on this enchantment.',
     'Remove five quest counters from this enchantment and sacrifice it: Search your library for an Equipment '
     'card, put it onto the battlefield, attach it to a creature you control, then shuffle.'): ('quest',(),()),
    ('Whenever you tap a land for mana, add one mana of any type that land produced. (The types of mana are '
     'white, blue, black, red, green, and colorless.)', CAST+'draw a card.'): ('resurgent',(),()),
    ('At the beginning of your upkeep, scry 1.', CAST+'you may pay {G}. If you do, draw a card.'):
        ('bestiary',('Scry',),(0,0,0,1,0,0)),
})
EXTRA.update({
    'recover': ('entry','unfunded','wrong_color'),
    'tap': ('own_target','already_tapped','no_target'),
    'quest': ('threshold','under_threshold'),
    'resurgent': ('land','foe_land','colorless','nonland'),
    'bestiary': ('unfunded','wrong_color','upkeep_keep','upkeep_bottom'),
})
FOLLOWER_EFFECTS = {'recover','tap','quest','resurgent','bestiary'}
WITNESSES |= {'Bonesplitter','Wastes','Replenish'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    role = RULES.get(tuple(record['oracle_text'].splitlines()))
    if (not role or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')):
        raise ValueError('complete reviewed normal creature-cast rider required')
    effect,keys,fee = role
    if sorted(record['keywords'])!=sorted(keys):
        raise ValueError('unreviewed additional keywords')
    return dict(effect=effect,price=cost_from_oracle(record),fee=list(fee or (0,)*6),keywords=list(keys))


def variants(shape):
    effect = shape['effect']
    modes = [f'{mode}_{seat}' for seat in (0,1) for mode in COMMON+EXTRA[effect]]
    return modes+(['decline_0'] if effect in ('loot','bounce','land','recover','quest','bestiary') else [])


def spec(declaration,record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profiles()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),shape)):
        raise ValueError('exact pinned creature-cast sources and whole Oracle required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct reviewed creature-cast variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data = profiles()
    if set(data['witnesses'])!=WITNESSES or not data['sources']:
        raise ValueError('complete pinned creature-cast witness scope required')
    for name,pin in (data['sources']|data['witnesses']).items():
        row = rows[name]
        if (not exact(pin['oracle'],oracle[name]) or not exact(pin['registry'],row)
                or row['kind'] not in (('card','token') if name==THRULL else ('card',))
                or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in STATIC):
            raise ValueError(f'creature-cast source or witness changed: {name}')
    if rows[THRULL]['csvid']!=18838:
        raise ValueError('native Thrull identity changed')


class CreatureCastBoard(CastTriggerBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.effect = shape['effect']
        self.counters = 0
        self.discards = 0
        self.search = None
        self.rebound = set()
        self.active = owner
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode,effect = self.mode,self.effect
        actor = self.foe if mode=='opponent' else self.owner
        witness = {'noncreature':'Reach Through Mists','zero':'Ornithopter',
                   'below_threshold':'Enormous Baloth','threshold':'Enormous Baloth'}.get(mode,'Grizzly Bears')
        if mode=='below_threshold':
            witness = 'Hill Giant'
        self.add(witness,'witness',actor,'hand')
        if mode=='reduction':
            self.add('Hill Giant','later',actor,'hand')
        if mode=='colored_floor':
            self.add('Elvish Mystic','later',actor,'hand')
        if mode=='below_threshold':
            self.add('Grizzly Bears','later',actor,'hand')
        if mode=='countered':
            self.add('Counterspell','response',self.foe,'hand')
        if effect=='bounce' and mode!='no_target':
            who = self.owner if mode=='own_target' else self.foe
            self.add('Hill Giant','victim',who)
            if mode=='foreign_owner':
                self.setup[-1].setdefault('state',[]).append('opponent_owned')
        if mode=='protection':
            for ref,name in (('white','Swords to Plowshares'),('black','Murder'),('red','Lightning Bolt')):
                self.add(name,ref,self.foe,'hand')
        if mode=='opposing_thrulls':
            for n in range(7):
                self.add(THRULL,f'remote_thrull_{n}',self.foe)
        if mode in ('devotion_low','devotion_high'):
            self.add('Leatherback Baloth','green_three',self.owner)
            if mode=='devotion_high':
                self.add("Garruk's Companion",'green_two',self.owner)
        if mode in ('devotion_low','devotion_high'):
            self.add('Unsummon','probe',self.foe,'hand')
        if mode=='indestructible':
            self.add('Vindicate','destroyer',self.foe,'hand')
        if effect=='land':
            name = {'typed_land':'Savannah','plains':'Plains'}.get(mode,'Forest')
            self.add(name,'search_land',self.owner,'library')
            self.add('Island','search_nonland_type',self.owner,'library')
            self.add('Forest','deck_foe',self.foe,'library')
        else:
            for who in (0,1):
                for n,name in enumerate(('Forest','Island','Plains')):
                    self.add(name,f'deck_{who}_{n}',who,'library')

    def stat(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def spend(self,who,price):
        pool = [have-due for have,due in zip(self.pools[who],price)]
        assert min(pool)>=0
        self.pools[who]=pool

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        amounts = Counter((card['player'],card['zone'],self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0,1):
            self.emit('assert_life',player=who,value=20)
            for zone in ZONES:
                self.emit('assert_zone',player=who,zone=zone,value=sum(qty for (seat,loc,_),qty in amounts.items()
                              if (seat,loc)==(who,zone)))
            ordered = [card['name'] for card in self.cards.values() if card['player']==who and card['zone']=='library']
            for n,name in enumerate(ordered):
                self.emit('assert_zone_card',player=who,zone='library',index=n,value=self.rows[name]['csvid'])
        for (who,zone,csvid),count in sorted(amounts.items()):
            # An incarnated Karametra retains the bound permanent but uses an
            # alternate native card-data row. Assert its type, stats and total zone count.
            if (self.effect=='land' and self.mode=='devotion_high' and zone=='battlefield'
                    and who==self.owner and csvid==self.rows[self.cards['source']['name']]['csvid']):
                continue
            self.emit('assert_zone_multiplicity',player=who,zone=zone,index=csvid,value=count)
        for ref,card in self.cards.items():
            if card['zone']!='battlefield' or card['name']==THRULL:
                continue
            if ref in self.rebound:
                self.emit('assert_matching_permanents',player=card['player'],index=self.rows[card['name']]['csvid'],
                          property='tapped',value=int(card['tapped']))
                continue
            self.stat(ref,'in_play',1)
            self.stat(ref,'tapped',card['tapped'])
            record = self.oracle[card['name']]
            if 'Creature' in types(record) and not (self.effect=='land' and ref=='source'):
                delta = self.counters if ref=='source' and self.effect=='counter' else card['boost']
                self.stat(ref,'power',int(record['power'])+delta+card.get('power_boost',0))
                self.stat(ref,'toughness',int(record['toughness'])+delta)
                self.stat(ref,'damage',card['damage'])
        if self.effect=='counter':
            self.emit('assert_counter',ref='source',counter_kind='plus1',value=self.counters)
        if self.effect=='loot':
            self.emit('assert_discard_choices',value=self.discards)
        if any(card['name']==THRULL and card['player']==self.owner for card in self.cards.values()):
            for prop,value in (('token',1),('color',2),('power',1),('toughness',1)):
                self.emit('assert_matching_permanents',player=self.owner,index=self.rows[THRULL]['csvid'],
                          property=prop,value=value)

    def advance(self):
        if any(map(any,self.pools)):
            raise ValueError('simulated turn needs zero owned mana after genuine payments')
        self.emit('advance_turn')
        self.active=1-self.active

    def stage_reward(self,board):
        assert board is self
        effect,mode = self.effect,self.mode
        if self.owner==0 and effect in ('loot','bounce','land') and mode!='decline':
            self.emit('choose_trigger_source',player=self.owner,ref='source')
        if mode=='decline':
            return
        if effect=='loot' and self.owner==0:
            if mode=='colored_fee':
                self.emit('choose_mana',player=0,color=2)
            elif mode=='unfunded':
                self.emit('choose_cancel_mana',player=0)
        if effect=='bounce' and mode not in ('unfunded','no_target'):
            self.emit('choose_target',player=self.owner,ref='victim')
        if effect=='land':
            self.emit('choose_zone_card',player=self.owner,value=self.rows[self.cards['search_land']['name']]['csvid'])

    def base_fee(self,ref):
        from recipes import cost_from_oracle

        record = self.oracle[self.cards[ref]['name']]
        return [0]*6 if record['mana_cost']=='{0}' else cost_from_oracle(record)

    def witness_price(self,ref):
        price = self.base_fee(ref)
        if self.effect=='counter' and self.cards[ref]['player']==self.owner and self.is_creature(ref):
            price[0] = max(0,price[0]-self.counters)
        return price

    def is_creature(self,ref):
        return 'Creature' in types(self.oracle[self.cards[ref]['name']])

    def activate_reward(self,ref,fee):
        mode,effect = self.mode,self.effect
        if mode=='decline':
            return
        if effect=='counter':
            self.counters += 1
            return
        if effect=='thrull':
            printed_value = int(self.oracle[self.cards[ref]['name']]['cmc'])
            quantity = sum(card['name']==THRULL and card['player']==self.owner for card in self.cards.values())
            for n in range(quantity,quantity+printed_value):
                self.cards[f'thrull_{n}'] = dict(name=THRULL,player=self.owner,zone='battlefield',tapped=False,damage=0,boost=0)
            if quantity+printed_value>=7:
                self.cards['source']['zone']='graveyard'
            return
        if effect in ('loot','bounce') and mode not in ('unfunded','no_target'):
            self.spend(self.owner,fee)
            if effect=='loot':
                self.draw(self.owner)
                top, = [card for card in self.cards.values() if card['player']==self.owner and card['zone']=='hand']
                top['zone']='graveyard'
                self.discards += int(self.owner==0)
            else:
                self.cards['victim']['zone']='hand'
                if mode=='foreign_owner':
                    self.cards['victim']['player']=self.owner
        if effect=='land':
            self.cards['search_land']['zone']='battlefield'
            self.cards['search_land']['tapped']=True
            self.rebound.add('search_land')
            self.search = self.cards['search_land']['name']

    def cast_witness(self,ref='witness'):
        mode,effect = self.mode,self.effect
        who = self.cards[ref]['player']
        applies = who==self.owner and self.is_creature(ref) and self.cards['source']['zone']=='battlefield'
        fee = list(self.shape['fee']) if applies and mode!='unfunded' else [0]*6
        if mode=='colored_fee':
            fee = [0,0,1,0,0,0]
        price = self.witness_price(ref)
        budget = [cost+extra for cost,extra in zip(price,fee)]
        if effect=='land' and applies and mode!='decline':
            self.emit('observe_search_filters',value=1)
        if price!=self.base_fee(ref) or not any(price):
            from recipes import COLORS
            self.fund(who,budget)
            self.emit('cast',ref=ref,value=1)
            self.emit('assert_stack',value=1)
            self.pay_cast(who,price,ref,COLORS)
            if applies:
                self.stage_reward(self)
            self.emit('announce_spell',ref=ref)
        else:
            self.cast_trigger_spell(ref,funding=budget,choices=self.stage_reward if applies else None,resolve=False)
        if applies:
            self.activate_reward(ref,fee)
        self.emit('assert_unused_choices',value=0)
        self.observe_mana()
        if self.search and applies:
            self.emit('assert_offered_count',value=2)
            self.emit('assert_offered_selection_required',value=0)
            self.emit('assert_offered_legal_card',index=self.rows[self.search]['csvid'],value=1)
            self.emit('assert_offered_legal_card',index=self.rows['Island']['csvid'],value=0)
        if mode=='countered':
            self.cast('response',response=True)
            self.emit('resolve_response')
            self.cards['response']['zone']=self.cards[ref]['zone']='graveyard'
        else:
            self.emit('resolve')
            self.cards[ref]['zone']='battlefield' if self.is_creature(ref) else 'graveyard'
            if not self.is_creature(ref):
                self.draw(who)
        self.emit('refresh')
        self.receipts()

    def pay_cast(self,who,price,ref,colors):
        self.spend(who,price)
        self.cards[ref]['zone']='stack'
        self.stat(ref,'color',sum(1<<colors[c] for c in self.oracle[self.cards[ref]['name']]['colors']))
        self.observe_mana()

    def verify_passive(self):
        mode = self.mode
        if mode=='protection':
            for ref,allowed in (('white',0),('black',0),('red',1)):
                self.emit('can_target_creature',ref=ref,other='source',value=allowed)
        if mode in ('devotion_low','devotion_high'):
            high = mode=='devotion_high'
            self.emit('can_target_creature',ref='probe',other='source',value=int(high))
            if high:
                self.stat('source','power',6)
                self.stat('source','toughness',7)
        if mode=='indestructible':
            self.advance()
            self.cast('destroyer',target='source')
            self.emit('resolve')
            self.emit('refresh')
            self.cards['destroyer']['zone']='graveyard'
            self.receipts()

    def play(self):
        self.play_source()
        if self.effect=='loot':
            self.emit('discard_policy',value=1)
        if self.mode=='opponent':
            self.advance()
        self.cast_witness()
        if self.mode=='colored_floor':
            self.cast('later',funding=[0]*6,allowed=False)
            self.receipts()
        if 'later' in self.cards:
            self.cast_witness('later')
        self.verify_passive()



class CreatureCastFollower(CreatureCastBoard):
    """Adjacent paid cast riders and their independent printed actions."""

    def build(self):
        mode,effect = self.mode,self.effect
        if effect=='resurgent':
            if mode=='own':
                return
            self.cards['source']['zone']=self.setup[0]['zone']='graveyard'
            self.add('Replenish','replenish',self.owner,'hand')
        actor = self.foe if mode=='opponent' else self.owner
        self.add('Reach Through Mists' if mode=='noncreature' else 'Grizzly Bears','witness',actor,'hand')
        if mode=='countered':
            self.add('Counterspell','response',self.foe,'hand')
        if effect=='recover' and mode!='entry':
            self.cards['source']['zone']=self.setup[0]['zone']='graveyard'
        if effect=='tap' and mode!='no_target':
            self.add('Hill Giant','victim',self.owner if mode=='own_target' else self.foe,
                     tapped=mode=='already_tapped')
        if effect=='quest' and mode in ('threshold','under_threshold'):
            limit = 5 if mode=='threshold' else 4
            for index in range(1,limit):
                self.add('Grizzly Bears',f'witness_{index}',self.owner,'hand')
            self.add('Hill Giant','equip_target',self.owner)
            self.add('Bonesplitter','equipment',self.owner,'library')
        if effect=='resurgent' and mode in ('land','foe_land','colorless','nonland'):
            name = 'Wastes' if mode=='colorless' else 'Elvish Mystic' if mode=='nonland' else 'Forest'
            self.add(name,'producer',self.foe if mode=='foe_land' else self.owner)
        if effect=='bestiary' and mode=='decline':
            self.add('Forest','optional_producer',self.owner)
        for who in (0,1):
            top = 'Lightning Bolt' if effect=='bestiary' and mode=='upkeep_bottom' and who==self.owner==1 else 'Forest'
            for n,name in enumerate((top,'Island','Plains')):
                self.add(name,f'deck_{who}_{n}',who,'library')

    def receipts(self):
        super().receipts()
        if self.effect=='quest' and self.cards['source']['zone']=='battlefield':
            self.emit('assert_counter',ref='source',counter_kind='quest',value=self.counters)

    def follow_choices(self,board):
        assert board is self
        effect,mode = self.effect,self.mode
        if effect=='recover':
            if self.owner==0 and mode not in ('unfunded','wrong_color'):
                self.emit('choose_dialog',player=self.owner,value=int(mode=='decline'))
        if effect=='tap' and mode!='no_target':
            self.emit('choose_target',player=self.owner,ref='victim')
        if effect=='quest' and self.owner==0 and mode!='decline':
            self.emit('choose_trigger_source',player=self.owner,ref='source')
        if effect=='bestiary' and self.owner==0 and mode in ('unfunded','wrong_color','decline'):
            self.emit('choose_cancel_mana',player=0)

    def follow_witness(self,ref='witness'):
        effect,mode = self.effect,self.mode
        actor = self.cards[ref]['player']
        applies = actor==self.owner and self.is_creature(ref)
        fee = [0]*6
        needs_cancel = effect=='bestiary' and mode=='decline'
        if applies and effect in ('recover','bestiary') and mode!='unfunded' and not needs_cancel:
            fee = [0,0,self.shape['fee'][3],0,0,0] if mode=='wrong_color' else self.shape['fee']
        budget = [n+added for n,added in zip(self.base_fee(ref),fee)]
        self.cast_trigger_spell(ref,funding=budget,choices=self.follow_choices if applies else None,resolve=False)
        if applies and mode!='decline':
            if effect=='quest':
                self.counters += 1
                self.emit('assert_counter',ref='source',counter_kind='quest',value=self.counters)
            if effect=='tap' and mode!='no_target':
                self.cards['victim']['tapped']=True
            if effect=='recover' and mode not in ('unfunded','wrong_color'):
                self.spend(self.owner,fee)
                self.cards['source']['zone']='hand'
            if effect=='bestiary' and mode not in ('unfunded','wrong_color'):
                self.spend(self.owner,fee)
                self.draw(self.owner)
            if effect=='resurgent':
                self.draw(self.owner)
        self.observe_mana()
        if mode=='countered':
            # This also observes an optional trigger wrongly left for entry.
            self.emit('assert_unused_choices',value=0)
            self.cast('response',response=True)
            self.emit('resolve_response')
            self.cards['response']['zone']=self.cards[ref]['zone']='graveyard'
        else:
            self.emit('resolve')
            self.cards[ref]['zone']='battlefield' if self.is_creature(ref) else 'graveyard'
            if not self.is_creature(ref):
                self.draw(actor)
        self.emit('assert_unused_choices',value=0)
        self.emit('refresh')
        self.receipts()

    def activate_quest(self):
        enough = self.mode=='threshold'
        self.emit('can_activate',ref='source',value=int(enough))
        self.emit('activate',ref='source',value=int(enough))
        self.emit('assert_stack',value=int(enough))
        if enough:
            self.cards['source']['zone']='graveyard'
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',
                      index=self.rows[self.cards['source']['name']]['csvid'],value=1)
            self.emit('choose_zone_card',player=self.owner,value=self.rows['Bonesplitter']['csvid'])
            self.emit('choose_target',player=self.owner,ref='equip_target')
            self.emit('resolve')
            self.emit('refresh')
            # The actual Equipment attachment changes the recipient without a separate equip payment.
            self.cards['equipment']['zone']='battlefield'
            self.cards['equip_target']['power_boost']=2
            self.rebound.add('equipment')
        self.receipts()

    def production(self):
        card = self.cards['producer']
        actor = card['player']
        color = 0 if card['name']=='Wastes' else 3
        expected = 1 if self.mode in ('foe_land','nonland') else 2
        self.emit('activate',ref='producer',value=1)
        self.pools[actor][color] += expected
        card['tapped']=True
        self.receipts()

    def upkeep_scry(self):
        self.advance()
        self.advance()
        self.emit('untap_step',player=self.owner)
        top = f'deck_{self.owner}_0'
        if self.mode=='upkeep_keep':
            self.emit('choose_zone',player=self.owner,value=-1)
        else:
            self.emit('choose_zone_card',player=self.owner,value=self.rows[self.cards[top]['name']]['csvid'])
            self.cards[top]=self.cards.pop(top)
        self.emit('upkeep_step',player=self.owner)
        self.emit('assert_unused_choices',value=0)
        self.receipts()

    def enter_resurgent(self):
        self.emit('turn',player=self.owner,value=0)
        self.fund(self.foe,[0]*6)
        self.cast('replenish')
        self.emit('resolve')
        self.emit('refresh')
        self.cards['replenish']['zone']='graveyard'
        self.cards['source']['zone']='battlefield'
        self.bind('source')

    def play(self):
        if self.effect=='resurgent' and self.mode=='own':
            # The direct cast is independently checked. Replenish cases
            # isolate the printed riders from the missing cast callback.
            self.emit('turn',player=self.owner,value=0)
            self.fund(self.foe,[0]*6)
            self.fund(self.owner,self.shape['price'])
            self.emit('cast',ref='source',value=1)
            self.emit('assert_unused_choices',value=0)
            return
        if self.effect=='resurgent':
            self.enter_resurgent()
        elif self.effect=='recover' and self.mode!='entry':
            self.emit('turn',player=self.owner,value=0)
            self.fund(self.foe,[0]*6)
            self.fund(self.owner,[0]*6)
            self.emit('prepare_graveyard_abilities')
        else:
            self.play_source()
        if self.mode=='entry':
            self.draw(self.owner)
            self.receipts()
            return
        if self.mode.startswith('upkeep_'):
            self.upkeep_scry()
            return
        if self.mode=='opponent':
            self.advance()
        self.follow_witness()
        if self.effect=='quest' and self.mode in ('threshold','under_threshold'):
            for ref in tuple(self.cards):
                if ref.startswith('witness_'):
                    self.follow_witness(ref)
            self.activate_quest()
        if self.effect=='resurgent' and 'producer' in self.cards:
            self.production()



def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('unbound creature-cast source or variant')
    gaps = ['Multiplayer and replacement effects.']
    if shape['effect']=='land':
        gaps.append('Bestowed Aura spell distinction and failing to find in a legal optional search are not calibrated.')
    if shape['effect'] in ('loot','bounce','land'):
        gaps.append('Optional decline is scripted for the human seat.')
    meta = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
    cases = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board_type = CreatureCastFollower if shape['effect'] in FOLLOWER_EFFECTS else CreatureCastBoard
        board = board_type(name,shape,mode,int(seat),rows,oracle)
        board.play()
        steps = board.steps
        if mode=='replay':
            steps = [step('capture'),*steps,step('record_state'),step('ai_roundtrip'),step('restore'),*steps,step('assert_recorded_state')]
        cases.append(dict(name=f'CastCreatureRiderRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2226,
                          setup=board.setup,steps=steps,_recipe=dict(meta,variant=variant)))
    return cases
