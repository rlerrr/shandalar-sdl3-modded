"""Spore or fade counters paid through real upkeep and token lifecycles."""

import copy
import json
from functools import lru_cache
from pathlib import Path

from attack_value_trigger_recipes import _AttackResourceWorld
from library_search_recipes import wave6_exact
from spore_upkeep_riders_recipes import BEAR, BOLT, COST, MAKE, TOKEN, UPKEEP, SporeWorld


FAMILY = 'spore_upkeep_additional_riders'
PROFILE = Path(__file__).parent / 'data/spore_upkeep_additional_riders_wave8_profiles.json'
MAKER = 'Thallid'
THELON = 'Thelon of Havenwood'
FERAL = 'Feral Thallid'
BURST = 'Saproling Burst'
BOUNCE = 'Unsummon'
BOUNCE_PERMANENT = 'Boomerang'
AURA = 'Regeneration'
FADING = ('Fading 7 (This enchantment enters with seven fade counters on it. At the beginning of your upkeep, '
          "remove a fade counter from it. If you can't, sacrifice it.)")
BURST_MAKE = ('Remove a fade counter from this enchantment: Create a green Saproling creature token. '
              'It has "This token\'s power and toughness are each equal to the number of fade counters on Saproling Burst."')
BURST_LEAVE = ("When this enchantment leaves the battlefield, destroy all tokens created with this enchantment. "
               "They can't be regenerated.")
SHAPES = {
    (UPKEEP,COST+'Regenerate this creature.'):('self_regen','feral_thallid',['Regenerate']),
    (UPKEEP,MAKE,'Sacrifice a Saproling: Regenerate target Fungus.'):('fungus_regen','savage_thallid',['Regenerate']),
    (UPKEEP,'Creatures you control have "Remove two spore counters from this creature: '
            'Create a 1/1 green Saproling creature token."'):('grant','sporoloth_ancient',[]),
    (FADING,BURST_MAKE,BURST_LEAVE):('fade','saproling_burst',['Fading']),
}
SCENES = {
    'self_regen':('short','threshold','bank','proactive','replay','recast'),
    'fungus_regen':('threshold','bank','proactive','own_fungus','enemy_fungus','nonfungus','replay'),
    'grant':('threshold','ally_paid','grant_bearer','enemy_only','removed_grant','replay'),
    'fade':('upkeep','two_tokens','last_counter','natural_fade','destroyed','bounced','no_regen','replay'),
}
WITNESSES = {TOKEN,THELON,MAKER,FERAL,BEAR,BOUNCE,BOUNCE_PERMANENT,AURA,BOLT}
REPLAY = {'replay'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    text=tuple(record['oracle_text'].splitlines())
    if (record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or text not in SHAPES):
        raise ValueError('exact supported complete upkeep/spending clauses required')
    kind,_,keys=SHAPES[text]
    stats={}
    if kind=='fade':
        if record['type_line']!='Enchantment':
            raise ValueError('a fading enchantment required')
    else:
        if record['type_line']!='Creature — Fungus':
            raise ValueError('an actual Fungus required')
        for key in ('power','toughness'):
            stats[key]=int(record[key])
    if record['keywords']!=keys:
        raise ValueError('printed upkeep/spending keywords differ')
    return dict(shape=kind,mana=cost_from_oracle(record),keywords=keys,**stats)


def handler(record):
    return 'manalinkeh_card_'+SHAPES[tuple(record['oracle_text'].splitlines())][1]


def variants(params):
    return [f'{mode}_{owner}' for mode in SCENES[params['shape']] for owner in (0,1)]


def spec(declaration,record):
    params=classify(record)
    selected=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not wave6_exact(profiles()['sources'].get(record['name'],{}).get('oracle'),record)
            or not wave6_exact(params,declaration.get('params')) or type(selected) is not list or not selected
            or any(type(v) is not str for v in selected) or len(set(selected))!=len(selected)
            or set(selected)-set(variants(params))):
        raise ValueError('exact whole printed spore/fade record, parameters and variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if (data['version']!=1 or not data['sources'] or set(data['witnesses'])!=WITNESSES):
        raise ValueError('exact pinned counter and regeneration witnesses needed')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row,record=rows[name],oracle[name]
            layout='token' if name==TOKEN else 'normal'
            if (not wave6_exact(pin,dict(oracle=record,registry=row)) or row['kind']!='card'
                    or record['layout']!=layout or row['handler'] in (None,'NULL','0')
                    or row['handler']=='card_dummy' and name!=BEAR):
                raise ValueError(f'changed source or witness: {name}')
            if group=='sources' and row['handler']!=handler(record):
                raise ValueError(f'changed reviewed spore/fade handler: {name}')
    token=oracle[TOKEN]
    if (rows[TOKEN]['csvid'],token['type_line'],token['power'],token['toughness'],token['colors'])!=(
            18813,'Token Creature — Saproling','1','1',['G']):
        raise ValueError('actual green Saproling token identity required')
    if (oracle[MAKER]['oracle_text']!=UPKEEP+'\n'+MAKE
            or not oracle[THELON]['oracle_text'].startswith('Each Fungus creature gets +1/+1 for each spore counter on it.')):
        raise ValueError('known threshold producer and independent P/T observer required')


class AdditionalWorld(SporeWorld):
    def __init__(self,name,params,mode,owner,rows,oracle):
        super().__init__(name,params,mode,owner,rows,oracle)
        self.spore_counts={}
        self.fade=None
        self.faded=[]
        self.source_zone='hand'
        self.bounce_spell=BOUNCE_PERMANENT if self.shape=='fade' else BOUNCE
        self.add(THELON,'thelon',self.owner) if self.shape!='fade' else None
        if (self.shape=='fungus_regen' and mode in ('own_fungus','enemy_fungus','nonfungus','replay')
                or self.shape=='grant' and mode in ('ally_paid','grant_bearer','removed_grant','replay')
                or self.shape=='fade' and mode in ('destroyed','bounced','no_regen')):
            self.add(MAKER,'maker',self.owner)
            self.spore_counts['maker']=0
        if self.shape=='fungus_regen':
            if mode in ('own_fungus','enemy_fungus','replay'):
                self.add(FERAL,'victim',self.foe if mode=='enemy_fungus' else self.owner)
                self.spore_counts['victim']=0
            elif mode=='nonfungus':
                self.add(BEAR,'victim',self.owner)
        if self.shape=='grant' and mode=='enemy_only':
            self.add(MAKER,'foreign',self.foe)
            self.spore_counts['foreign']=0
        if mode in ('bounced','recast'):
            self.add(self.bounce_spell,'reply',self.foe,'hand')
        if self.shape=='fade' and mode=='no_regen':
            self.add(AURA,'aura',self.owner,'hand')
            self.add(BOLT,'bolt',self.owner,'hand')

    def stats(self):
        for ref,entity in self.entities.items():
            record=self.o[entity['name']]
            if ref in self.faded:
                value=self.fade
                entity.update(power=value,toughness=value)
            else:
                extra=self.spore_counts.get(ref,0) if self.shape!='fade' else 0
                entity.update(power=int(record['power'])+extra,toughness=int(record['toughness'])+extra)

    def receipts(self):
        self.stats()
        start=len(self.steps)
        _AttackResourceWorld.receipts(self)
        # The chosen Saproling identity can differ under native AI cost policy.
        # Compare exact token counts/characteristics and receipt of the targeted Fungus.
        if self.shape=='fungus_regen' and self.mode in ('own_fungus','enemy_fungus','nonfungus','replay'):
            self.steps[start:]=[step for step in self.steps[start:] if not step.get('ref','').startswith('saproling_')]
        if self.source_zone=='battlefield' and self.fade is not None:
            self.observe('source','in_play',1)
            self.emit('assert_counter',ref='source',counter_kind='fade',value=self.fade)
        for who in (0,1):
            for zone in ('battlefield','graveyard','exile'):
                self.zone_count(TOKEN,who,zone,self.zones[zone][who].count(TOKEN))
            if TOKEN in self.zones['battlefield'][who]:
                for prop,val in (('token',1),('artifact',0),('color',8),('subtype_Saproling',1)):
                    self.emit('assert_matching_permanents',player=who,index=self.rows[TOKEN]['csvid'],property=prop,value=val)
                if self.shape!='fade':
                    for prop in ('power','toughness'):
                        self.emit('assert_matching_permanents',player=who,index=self.rows[TOKEN]['csvid'],property=prop,value=1)

    def cast_fixed(self,name,ref,actor,target=None):
        from recipes import cost_from_oracle

        self.pool(actor,cost_from_oracle(self.o[name]))
        self.emit('refresh')
        if target:
            self.emit('choose_target',player=actor,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.emit('assert_unused_choices',value=0)
        self.emit('require_card',ref=ref,property='in_hand',value=0)
        self.emit('resolve')
        self.mana[actor]=[0]*6

    def cast_main(self):
        self.cast_fixed(self.name,'source',self.owner)
        self.source_zone='battlefield'
        if self.shape=='fade':
            self.zones['hand'][self.owner].remove(self.name)
            self.zones['battlefield'][self.owner].append(self.name)
            self.fade=7
        else:
            self.arrive('source')
            self.spore_counts['source']=0
        self.receipts()

    def lose_source(self,to_zone):
        if self.shape=='fade':
            self.zones['battlefield'][self.owner].remove(self.name)
            self.zones[to_zone][self.owner].append(self.name)
            for ref in self.faded:
                if self.entities[ref]['zone']=='battlefield':
                    self.leave(ref,'graveyard')
        else:
            self.leave('source',to_zone)
            del self.spore_counts['source']
        self.source_zone=to_zone

    def upkeep_turn(self):
        if any(map(any,self.mana)):
            raise ValueError('true upkeep requires empty pools')
        self.emit('advance_turn')
        self.expire()
        self.current=1-self.current
        self.emit('untap_step',player=self.current)
        for e in self.entities.values():
            if e['zone']=='battlefield' and e['player']==self.current:
                e.update(tapped=False,sick=False)
        self.emit('upkeep_step',player=self.current)
        for ref in self.spore_counts:
            self.spore_counts[ref]+=int(self.entities[ref]['player']==self.current)
        if self.shape=='fade' and self.source_zone=='battlefield' and self.current==self.owner:
            if self.fade:
                self.fade-=1
                self.prune_zero()
            else:
                self.lose_source('graveyard')
        self.emit('assert_unused_choices',value=0)
        self.emit('turn',player=self.current,value=0)
        self.emit('refresh')
        self.receipts()

    def prune_zero(self):
        if self.fade==0:
            for ref in self.faded:
                if self.entities[ref]['zone']=='battlefield':
                    self.leave(ref,'graveyard')

    def make_token(self,source,amount,donor=None):
        self.emit('require_can_activate',ref=source,value=1)
        if donor is not None:
            self.emit('choose_target',player=self.owner,ref=donor)
        if self.shape=='fungus_regen' and source=='source':
            self.mode_intent('Create a Saproling')
        self.emit('activate',ref=source,value=1)
        if self.shape=='fungus_regen' and source=='source':
            self.emit('require_activation_mode',value=1)
        self.emit('assert_stack',value=1)
        self.emit('assert_unused_choices',value=0)
        if donor is not None or self.fade is None or source!='source':
            payer=source if donor is None else donor
            self.spore_counts[payer]-=amount
            self.stats()
            self.observe(payer,'power',self.entities[payer]['power'])
        else:
            self.fade-=amount
            self.emit('assert_counter',ref='source',counter_kind='fade',value=self.fade)
            self.prune_zero()
        count=self.zones['battlefield'][self.owner].count(TOKEN)
        self.zone_count(TOKEN,self.owner,'battlefield',count)
        self.emit('resolve')
        self.emit('refresh')
        if not (self.shape=='fade' and source=='source' and not self.fade):
            ref=f'saproling_{self.created}'
            self.created+=1
            self.entity(TOKEN,ref,self.owner,'battlefield',token=True)
            self.zones['battlefield'][self.owner].append(TOKEN)
            self.emit('bind_permanent_copy',player=self.owner,value=self.rows[TOKEN]['csvid'],index=count,ref=ref)
            if self.shape=='fade' and source=='source':
                self.faded.append(ref)
        self.receipts()

    def bounce(self):
        self.cast_fixed(self.bounce_spell,'reply',self.foe,'source')
        self.zones['hand'][self.foe].remove(self.bounce_spell)
        self.zones['graveyard'][self.foe].append(self.bounce_spell)
        self.lose_source('hand')
        self.receipts()

    def self_regenerate(self,amount):
        self.emit('choose_regeneration',ref='source')
        self.emit('destroy',ref='source')
        self.emit('assert_unused_choices',value=int(not amount))
        if amount:
            self.spore_counts['source']-=amount
            self.entities['source'].update(tapped=True,damage=0,attacking=False)
        else:
            self.lose_source('graveyard')
        self.receipts()

    def savage_regenerate(self):
        target='victim'
        eligible=self.mode!='nonfungus'
        self.emit('choose_regeneration',ref='source')
        if eligible:
            # Native destruction priority calls the sacrifice handler before
            # its display. This attempt requests the real target and one cost.
            self.emit('choose_target',player=self.owner,ref=target)
            if self.owner==0:
                self.emit('choose_target',player=0,ref='saproling_0')
        self.emit('destroy',ref=target)
        self.emit('assert_unused_choices',value=int(not eligible))
        if eligible:
            self.leave('saproling_0','graveyard')
            self.entities[target].update(tapped=True,damage=0,attacking=False)
        else:
            self.leave(target,'graveyard')
        self.receipts()

    def burst_prevents_regen(self):
        from recipes import cost_from_oracle

        # The same aura really regenerates a linked Saproling from ordinary
        # lethal damage while the Burst still has a positive fade total.
        target='saproling_1'
        self.cast_fixed(AURA,'aura',self.owner,target)
        self.zones['hand'][self.owner].remove(AURA)
        self.zones['battlefield'][self.owner].append(AURA)
        self.pool(self.owner,[0,0,0,1,0,0])
        self.emit('refresh')
        self.emit('choose_regeneration',ref='aura')
        self.emit('destroy',ref=target)
        self.emit('assert_unused_choices',value=0)
        self.mana[self.owner]=[0]*6
        self.entities[target]['tapped']=True
        self.receipts()
        assert cost_from_oracle(self.o[AURA])==[1,0,0,1,0,0]
        self.pool(self.owner,[0,0,0,1,0,0])
        self.emit('refresh')
        self.emit('choose_regeneration',ref='aura')
        self.emit('destroy',ref='source')
        self.emit('assert_unused_choices',value=1)
        self.lose_source('graveyard')
        self.zones['battlefield'][self.owner].remove(AURA)
        self.zones['graveyard'][self.owner].append(AURA)
        self.receipts()

    def spore_cases(self):
        mode=self.mode
        if self.shape=='self_regen':
            self.own_upkeeps(2 if mode in ('short','threshold') else 4 if mode=='bank' else 3)
            if mode=='proactive':
                self.emit('can_activate',ref='source',value=1)
            elif mode=='recast':
                self.bounce()
                self.emit('bind_hand',player=self.owner,value=self.rows[self.name]['csvid'],ref='source')
                self.cast_main()
                self.own_upkeeps(2)
                self.self_regenerate(0)
            elif mode=='threshold':
                self.upkeep_turn()
                self.upkeep_turn()
                self.self_regenerate(3)
            else:
                self.self_regenerate(0 if mode=='short' else 3)
        elif self.shape=='fungus_regen':
            self.own_upkeeps(4 if mode=='bank' else 2)
            if mode!='bank':
                self.emit('can_activate',ref='source',value=0)
                self.own_upkeeps(1)
            self.make_token('source',3)
            if mode=='proactive':
                self.emit('can_activate',ref='source',value=1)
            if 'maker' in self.spore_counts:
                self.make_token('maker',3)
                self.savage_regenerate()
        elif mode=='enemy_only':
            self.own_upkeeps(1)
            self.upkeep_turn()
            self.emit('can_activate',ref='source',value=0)
        else:
            self.own_upkeeps(1)
            self.emit('can_activate',ref='source',value=0)
            self.own_upkeeps(1)
            if mode=='grant_bearer':
                self.emit('can_activate',ref='maker',value=1)
            elif mode=='removed_grant':
                self.emit('destroy',ref='source')
                self.lose_source('graveyard')
                self.receipts()
                self.emit('can_activate',ref='maker',value=0)
                self.own_upkeeps(1)
                self.make_token('maker',3)
            else:
                self.make_token('source',2,donor='maker' if 'maker' in self.spore_counts else 'source')

    def fade_cases(self):
        mode=self.mode
        if mode=='natural_fade':
            self.own_upkeeps(7)
            self.emit('can_activate',ref='source',value=0)
            self.own_upkeeps(1)
            return
        if mode=='upkeep':
            self.make_token('source',1)
            self.own_upkeeps(2)
            return
        if mode=='last_counter':
            self.own_upkeeps(6)
            self.make_token('source',1)
            self.emit('can_activate',ref='source',value=0)
            self.own_upkeeps(1)
            return
        if 'maker' in self.spore_counts:
            self.own_upkeeps(3)
            self.make_token('maker',3)
        self.make_token('source',1)
        if mode in ('two_tokens','replay'):
            self.make_token('source',1)
            self.own_upkeeps(1)
        if mode=='destroyed':
            self.emit('destroy',ref='source')
            self.lose_source('graveyard')
            self.receipts()
        if mode=='bounced':
            self.bounce()
        if mode=='no_regen':
            self.burst_prevents_regen()


def expand(name,csvid,declaration,params,selected,rows,oracle):
    from recipes import step

    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(params)):
        raise ValueError('approved counter/token lifecycle variants required')
    validate_witnesses(oracle,rows)
    gaps=['Spore totals are observed through Thelon’s actual printed P/T effect and repeated legal spends.']
    if params['shape']=='grant':
        gaps.append('The legacy output path selects the payer via Ancient; the printed bearer source is checked separately.')
    if params['shape']=='fade':
        gaps=['Fade observations, distinct token identity and actual growth/leave receipts exercise the printed linkage.']
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        w=AdditionalWorld(name,params,mode,int(seat),rows,oracle)
        w.emit('turn',player=w.owner,value=0)
        for who in (0,1):
            w.pool(who,[0]*6)
        w.cast_main()
        cut=len(w.steps)
        if w.shape=='fade':
            w.fade_cases()
        else:
            w.spore_cases()
        if mode in REPLAY:
            tail=w.steps[cut:]
            w.steps=w.steps[:cut]+[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),step('restore'),
                                    *copy.deepcopy(tail),step('assert_recorded_state')]
        cases.append(dict(name=f'SporeUpkeepAdditionalRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=123,setup=w.setup,steps=w.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
