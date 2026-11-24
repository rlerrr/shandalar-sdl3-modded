"""Exercise real alternate Kicker costs, damage, and priority restrictions."""

from functools import lru_cache
import json
from pathlib import Path
import re

from library_search_recipes import wave6_exact


FAMILY = 'kicker_alternate_payment_and_damage'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
BLOOD, PET, MOLTEN, ORIM, FOOTING, RAGE = (10509,5224,8977,5411,10699,5173)
EXTRAS = {
    BLOOD: ('no_vampire','paid_even'),
    PET: ('blue_fee','paid_buffer','base_only'),
    MOLTEN: ('lockout','base_response','mana_response'),
    ORIM: ('enchantment','no_creature'),
    FOOTING: ('walker','shield_only','paid_unshielded','declined_unshielded'),
    RAGE: ('counter_attempt','paid_unshielded','declined_unshielded'),
}
PAY_MODES = {'paid','lockout','enchantment','walker','counter_attempt','paid_even','paid_buffer',
             'mana_response','paid_unshielded'}
NO_OFFER = {'no_vampire','no_creature','base_only'}
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
MODELS = {
    'Blood Tribute': (BLOOD, 'blood_tribute'),
    "Dralnu's Pet": (PET, 'dralnus_pet'),
    'Molten Disaster': (MOLTEN, 'molten_disaster'),
    "Orim's Thunder": (ORIM, 'orims_thunder'),
    'Unstable Footing': (FOOTING, 'unstable_footing'),
    "Urza's Rage": (RAGE, 'urzas_rage'),
}
TAP_FEE = 'Kicker—Tap an untapped Vampire you control. (You may tap a Vampire you control in addition to any other costs as you cast this spell.)'
DISCARD_FEE = 'Kicker—{2}{B}, Discard a creature card. (You may pay {2}{B} and discard a creature card in addition to any other costs as you cast this spell.)'
ZONES = ('hand','battlefield','library','graveyard','exile')
SUPPORT = {'Counterspell','Enormous Baloth','Giant Spider','Grizzly Bears','Hill Giant','Icy Manipulator','Island',
           'Jace Beleren','Lightning Bolt','Plains','Safe Passage','Sanguine Bond','Vampire Lacerator','Wall of Fire',
           'Wind Drake','Shock'}
VANILLA = {'Enormous Baloth','Giant Spider','Grizzly Bears','Hill Giant','Island','Plains','Wind Drake'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    matched = MODELS.get(record['name'])
    pin = profiles()['sources'].get(record['name'])
    if not matched or not pin or not wave6_exact(record,pin['oracle']) or record['layout']!='normal':
        raise ValueError('complete and exact alternate Kicker Oracle record required')
    csvid,_ = matched
    printed = record['oracle_text'].splitlines()[0]
    if csvid == BLOOD:
        assert printed == TAP_FEE
        fee = [0]*6
    elif csvid == PET:
        assert printed == DISCARD_FEE
        fee = cost_from_oracle({'mana_cost':'{2}{B}'})
    else:
        match = KICKER.fullmatch(printed)
        if not match:
            raise ValueError('unreviewed mana Kicker cost')
        fee = cost_from_oracle({'mana_cost':match[1]})
    mana = cost_from_oracle({'mana_cost':record['mana_cost'].replace('{X}','{3}')})
    return dict(model=csvid,mana=mana,kicker_mana=fee,x=3 if csvid==MOLTEN else None)


def handler(record):
    return 'manalinkeh_card_' + MODELS[record['name']][1]


def variants(shape):
    return [f'{mode}_{actor}' for mode in ('paid','declined',*EXTRAS[shape['model']]) for actor in (0,1)]


def spec(declaration, record):
    shape = classify(record)
    chosen = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('exact printed alternate payment declaration and approved variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle, rows):
    all_pins=profiles()
    if set(all_pins['sources'])!=MODELS.keys() or all_pins['version']!=1 or set(all_pins['witnesses'])!=SUPPORT:
        raise ValueError('complete alternate Kicker source and witness profiles required')
    for group in ('sources','witnesses'):
        for name,pin in all_pins[group].items():
            record,row=oracle[name],rows[name]
            if (not wave6_exact(pin,dict(oracle=record,registry=row)) or row['kind']!='card' or record['layout']!='normal'
                    or row['handler'] in ('NULL','0',None) or (row['handler']=='card_dummy' and name not in VANILLA)):
                raise ValueError(f'changed exact Kicker witness or source {name}')
            if group=='sources' and row['handler']!=handler(record):
                raise ValueError(f'changed Kicker source handler {name}')


class AlternateDamage:
    def __init__(self,record,shape,actor,mode,rows,oracle):
        self.source,self.shape,self.csvid=record,shape,shape['model']
        self.actor,self.enemy,self.mode=actor,1-actor,mode
        self.paid=mode in PAY_MODES
        self.setup,self.steps=[],[]
        self.rows,self.oracle=rows,oracle
        self.life=[20,20]
        self.add('source',actor,record['name'],0)
        for seat in (0,1):
            self.add(f'deck_{seat}_0',seat,'Island',2)
            self.add(f'deck_{seat}_1',seat,'Plains',2)
        self.build()

    def add(self,ref,actor,name,zone=1):
        from recipes import card

        self.setup.append(card(name,ref,actor,ZONES[zone]))

    def emit(self,op,ref='',other='',property_name='',*,zone=None,**fields):
        from recipes import step

        if ref:
            fields['ref']=ref
        if other:
            fields['other']=other
        if property_name:
            fields['counter_kind' if op=='assert_counter' else 'property']=property_name
        if zone is not None:
            fields['zone']=ZONES[zone]
        self.steps.append(step(op,**fields))

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref,property_name=prop,value=value)

    def mana(self,op,actor,pool):
        for color,amount in enumerate(pool):
            self.emit(op,player=actor,value=amount,color=color)

    def zone(self,name,actor,zone,amount):
        self.emit('assert_zone_multiplicity',player=actor,zone=zone,index=self.rows[name]['csvid'],value=amount)

    def shielded(self):
        return self.mode not in ('counter_attempt','paid_unshielded','declined_unshielded')

    def build(self):
        card = self.csvid
        if card==BLOOD:
            self.add('bolt',self.actor,'Shock' if self.mode=='paid_even' else 'Lightning Bolt',0)
            if self.mode!='no_vampire':
                self.add('vampire',self.actor,'Vampire Lacerator',0)
            self.add('foe_vampire',self.enemy,'Vampire Lacerator')
            self.add('foil_creature',self.actor,'Grizzly Bears')
        elif card==PET:
            self.add('discard',self.actor,'Hill Giant',0)
            self.add('keep',self.actor,'Island',0)
            self.add('ground',self.enemy,'Grizzly Bears')
            self.add('air',self.enemy,'Wind Drake')
        elif card==MOLTEN:
            for seat in (0,1):
                self.add(f'bear_{seat}',seat,'Grizzly Bears')
                self.add(f'spider_{seat}',seat,'Giant Spider')
                self.add(f'flyer_{seat}',seat,'Wind Drake')
            if self.mode in ('lockout','base_response'):
                self.add('response',self.enemy,'Wall of Fire')
            if self.mode=='mana_response':
                self.add('mana_land',self.enemy,'Island')
        elif card==ORIM:
            self.target='Sanguine Bond' if self.mode=='enchantment' else 'Icy Manipulator'
            self.add('target',self.enemy,self.target)
            if self.mode!='no_creature':
                self.add('creature',self.actor,'Enormous Baloth')
                self.add('foil',self.enemy,'Grizzly Bears')
        elif card in (FOOTING,RAGE):
            if self.shielded():
                self.add('shield',self.enemy,'Safe Passage',0)
            if card==FOOTING:
                self.add('bolt',self.actor,'Lightning Bolt',0)
                if self.mode=='walker':
                    self.add('walker',self.enemy,'Jace Beleren',0)
            if self.mode=='counter_attempt':
                self.add('counter',self.enemy,'Counterspell',0)

    def pools(self, actor, pool):
        self.mana('set_mana',actor,pool)
        self.emit('refresh')

    def plain_spell(self,ref,actor,name,*,recipient=None):
        from recipes import cost_from_oracle

        self.pools(actor,cost_from_oracle(self.oracle[name]))
        if recipient is not None:
            self.emit('choose_player',player=actor,value=recipient)
        self.emit('cast',ref,value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        self.mana('assert_mana',actor,[0]*6)

    def assert_life(self):
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=self.life[seat])

    def preliminaries(self):
        self.emit('turn',player=self.enemy if self.mode=='walker' else self.actor)
        for seat in (0,1):
            self.pools(seat,[0]*6)
        if self.csvid==BLOOD:
            odd = self.mode!='paid_even'
            self.plain_spell('bolt',self.actor,'Lightning Bolt' if odd else 'Shock',recipient=self.enemy)
            self.life[self.enemy]-=3 if odd else 2
            self.assert_life()
            if self.mode!='no_vampire':
                self.plain_spell('vampire',self.actor,'Vampire Lacerator')
                self.observe('vampire','in_play',1)
                self.observe('vampire','sick',1)
                self.observe('vampire','tapped',0)
            else:
                self.observe('foil_creature','in_play',1)
            self.observe('foe_vampire','tapped',0)
        if self.mode=='walker':
            self.plain_spell('walker',self.enemy,'Jace Beleren')
            self.emit('assert_counter','walker',property_name='loyalty',value=3)
            self.emit('advance_turn')
        if self.csvid in (FOOTING,RAGE) and self.shielded():
            self.plain_spell('shield',self.enemy,'Safe Passage')

    def cast_source(self):
        base_pool=self.shape['mana']
        fee=self.shape['kicker_mana']
        supplied=list(fee)
        if self.mode=='base_only':
            supplied=[0]*6
        if self.mode=='blue_fee':
            supplied[1],supplied[2]=0,supplied[1]
        if self.mode=='paid_buffer':
            supplied[2]+=1
        remaining=[0]*6 if self.paid else supplied
        if self.mode=='paid_buffer':
            remaining[2]=1
        self.pools(self.actor,[a+b for a,b in zip(base_pool,supplied)])
        if self.csvid==MOLTEN:
            self.emit('choose_x',player=self.actor,value=3)
        if self.csvid==ORIM:
            self.emit('choose_target','target',player=self.actor)
        if self.csvid==RAGE:
            self.emit('choose_player',player=self.actor,value=self.enemy)
        if self.mode not in NO_OFFER:
            self.emit('choose_dialog',player=self.actor,value=int(not self.paid))
        if self.paid:
            if self.csvid==BLOOD:
                self.emit('choose_target','vampire',player=self.actor)
            elif self.csvid==PET:
                self.emit('choose_zone_card',player=self.actor,value=self.rows['Hill Giant']['csvid'])
            elif self.csvid==ORIM:
                self.emit('choose_target','creature',player=self.actor)
            elif self.csvid==FOOTING:
                if self.mode=='walker':
                    self.emit('choose_target','walker',player=self.actor)
                else:
                    self.emit('choose_player',player=self.actor,value=self.enemy)
        self.emit('cast','source',value=1)
        if self.csvid==PET:
            # End the optional plan even when the legacy fee path suppresses
            # the actual offer. The Oracle pool and target still stay exact.
            self.emit('assert_unused_choices',value=int(self.mode=='blue_fee'))
        else:
            self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.mana('assert_mana',self.actor,remaining)
        self.mana('assert_mana',self.enemy,[0]*6)
        return remaining

    def pet_receipts(self):
        size=int(self.oracle['Hill Giant']['cmc']) if self.paid else 0
        self.observe('source','in_play',1)
        self.emit('assert_counter','source',property_name='plus1',value=size)
        self.observe('source','power',2+size)
        self.observe('source','toughness',2+size)
        self.zone('Hill Giant',self.actor,0,int(not self.paid))
        self.zone('Hill Giant',self.actor,3,int(self.paid))
        self.zone('Island',self.actor,0,1)
        self.zone('Island',self.actor,3,0)
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('begin_attackers')
        self.emit('attack','source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block','ground','source',value=int(not self.paid))
        if self.paid:
            # The separate flying blocker must still be legal.
            self.emit('block','air','source',value=1)

    def molten_receipts(self, *, after):
        self.assert_life()
        for seat in (0,1):
            self.zone('Grizzly Bears',seat,1,int(not after))
            self.zone('Grizzly Bears',seat,3,int(after))
            self.observe(f'spider_{seat}','in_play',1)
            self.observe(f'spider_{seat}','damage',3 if after else 0)
            self.observe(f'flyer_{seat}','in_play',1)
            self.observe(f'flyer_{seat}','damage',0)

    def run(self):
        from recipes import cost_from_oracle

        self.preliminaries()
        if self.mode=='shield_only':
            self.plain_spell('bolt',self.actor,'Lightning Bolt',recipient=self.enemy)
            self.assert_life()
            self.zone(self.source['name'],self.actor,0,1)
            return

        remaining=self.cast_source()
        if self.csvid==BLOOD and self.mode!='no_vampire':
            self.observe('vampire','tapped',int(self.paid))
            self.observe('foe_vampire','tapped',0)
        if self.csvid==PET:
            self.zone('Hill Giant',self.actor,3,int(self.paid))
        if self.csvid==MOLTEN:
            self.molten_receipts(after=False)
            if self.mode=='mana_response':
                self.emit('announce_spell','source')
                self.emit('activate_response','mana_land',value=1)
                self.emit('assert_stack',value=1)
                self.observe('mana_land','tapped',1)
                self.mana('assert_mana',self.enemy,[0,0,1,0,0,0])
            if self.mode in ('lockout','base_response'):
                self.emit('announce_spell','source')
                allowed=self.mode=='base_response'
                fee_pool=cost_from_oracle({'mana_cost':'{R}'})
                self.pools(self.enemy,fee_pool)
                self.emit('activate_response','response',value=int(allowed))
                self.emit('assert_stack',value=1+int(allowed))
                self.observe('response','in_play',1)
                self.mana('assert_mana',self.enemy,[0]*6 if allowed else fee_pool)
                if self.mode=='lockout':
                    return
        if self.mode=='counter_attempt':
            self.emit('announce_spell','source')
            self.pools(self.enemy,cost_from_oracle(self.oracle['Counterspell']))
            self.emit('cast_response','counter',value=1)
            self.emit('assert_stack',value=2)

        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        self.mana('assert_mana',self.actor,remaining)
        if self.csvid!=PET:
            self.zone(self.source['name'],self.actor,3,1)
        if self.csvid==BLOOD:
            lost=(self.life[self.enemy]+1)//2
            self.life[self.enemy]-=lost
            self.life[self.actor]+=lost*int(self.paid)
            self.assert_life()
            if self.mode!='no_vampire':
                self.observe('vampire','tapped',int(self.paid))
            self.observe('foe_vampire','tapped',0)
        elif self.csvid==PET:
            self.pet_receipts()
        elif self.csvid==MOLTEN:
            self.life=[amount-3 for amount in self.life]
            self.molten_receipts(after=True)
            if self.mode=='base_response':
                self.observe('response','power',1)
                self.observe('response','toughness',5)
                self.observe('response','damage',3)
        elif self.csvid==ORIM:
            self.zone(self.target,self.enemy,3,1)
            self.zone(self.target,self.enemy,1,0)
            if self.mode!='no_creature':
                self.observe('creature','damage',int(self.oracle[self.target]['cmc']) if self.paid else 0)
                self.observe('foil','damage',0)
        elif self.csvid==FOOTING:
            if self.mode=='walker':
                self.zone('Jace Beleren',self.enemy,3,1)
                self.zone('Jace Beleren',self.enemy,1,0)
            elif self.paid:
                self.life[self.enemy]-=5
            self.assert_life()
            self.plain_spell('bolt',self.actor,'Lightning Bolt',recipient=self.enemy)
            self.life[self.enemy]-=3
            self.assert_life()
        elif self.csvid==RAGE:
            if self.mode=='counter_attempt':
                self.zone('Counterspell',self.enemy,0,0)
                self.zone('Counterspell',self.enemy,3,1)
                self.mana('assert_mana',self.enemy,[0]*6)
            self.life[self.enemy]-=10 if self.paid else 0 if self.shielded() else 3
            self.assert_life()


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    out=[]
    for label in selected:
        mode,actor=label.rsplit('_',1)
        board=AlternateDamage(oracle[name],shape,int(actor),mode,rows,oracle)
        board.run()
        out.append(dict(name=f'KickerAlternatePaymentAndDamageRecipe_{csvid}_{label}',card=name,fixture='empty_main',seed=2334,
                        setup=board.setup,steps=board.steps,_recipe=dict(family=FAMILY,version=2,variant=label,
                            params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=[])))
    return out
