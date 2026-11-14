"""Typed finite-prefix combat rewards, with independent ordered-zone receipts."""
import copy
import json
import re
from pathlib import Path

from return_saboteur_trigger_recipes import ReturnBoard
from combat_value_trigger_recipes import _mirror
from defensive_creature_recipes import reviewed_payment

FAMILY = 'combat_library_prefix'
PROFILE_PATH = Path(__file__).parent/'data/combat_library_prefix_wave8_profiles.json'
TAIL = ['Mountain','Sol Ring','Island','Giant Growth','Swamp']
NUMBERS = {'one':1,'two':2,'three':3,'four':4}


def profiles(): return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(r):
    if (not isinstance(r,dict) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line','') or any(not re.fullmatch(r'[1-8]',r.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('prefix rewards require fixed normal creature characteristics')
    alias='(?:'+'|'.join(re.escape(n) for n in sorted({'this creature',r['name'],r['name'].split(',')[0]}))+')'
    keys=[]; trigger=None; shield=None; condition=None; delirium=False; land_damage=False
    for line in r['oracle_text'].splitlines():
        if line in ('Flying','Trample'): keys.append(line);continue
        if line=="Flying (This creature can't be blocked except by creatures with flying or reach.)":keys.append('Flying');continue
        if line=="Hexproof (This creature can't be the target of spells or abilities your opponents control.)":shield='always';keys.append('Hexproof');continue
        if re.fullmatch(alias+r" has hexproof as long as it's untapped\.",line):shield='untapped';continue
        if line=='Delirium — This creature gets +2/+2 as long as there are four or more card types among cards in your graveyard.':delirium=True;keys.append('Delirium');continue
        if re.fullmatch(r'Discard a land card: '+alias+r' deals 3 damage to any target\.',line):land_damage=True;continue
        m=re.fullmatch(r'Whenever '+alias+r' deals combat damage to a player, (if you control no other creatures, )?(.+)',line)
        if not m or trigger is not None:raise ValueError('additional or duplicate prefix clause requires another model')
        condition='alone' if m[1] else None;trigger=m[2]
    kind=None;window=None
    if trigger=='reveal the top card of your library and put that card into your hand. You gain life equal to its mana value.':kind='top_life';window=1
    m=re.fullmatch(r'look at the top (three|four) cards of your library\. Put one of them into your hand and the rest on the bottom of your library in any order\.',trigger or '')
    if m:kind='one_bottom';window=NUMBERS[m[1]]
    if trigger=='reveal the top three cards of your library. Put all land cards revealed this way into your hand and the rest into your graveyard.':kind='land_partition';window=3
    if trigger=='that player reveals cards from the top of their library until they reveal four land cards, then puts those cards into their graveyard.':kind='until_lands';window=4
    if trigger=='that player exiles the top four cards of their library. If two or more of those cards have the same name, repeat this process.':kind='repeat_exile';window=4
    if trigger=='scry 2. (Look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.)':kind='scry';window=2;keys.append('Scry')
    if trigger=='surveil 3. (Look at the top three cards of your library, then put any number of them into your graveyard and the rest on top of your library in any order.)':kind='surveil';window=3;keys.append('Surveil')
    if kind is None or set(keys)!=set(r.get('keywords',[])) or len(keys)!=len(set(keys)):
        raise ValueError('complete prefix text and keyword metadata must agree')
    if condition and kind!='one_bottom' or delirium and kind!='surveil' or land_damage and kind!='land_partition':
        raise ValueError('unreviewed prefix companion clause combination')
    p=dict(kind=kind,window=window,keywords=sorted(keys),shield=shield,condition=condition,delirium=delirium,land_damage=land_damage)
    pairs=re.findall(r'\{([WUBRG])/([WUBRG])\}',r['mana_cost'])
    if pairs:p['payment_colors']=[a for a,b in pairs]
    return p


def spec(d,r):
    p=parameters(r);profile=profiles()['sources'].get(r['name'])
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
            or not profile or profile['oracle']!=r or profile['params']!=p
            or d.get('oracle_text')!=r['oracle_text'] or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(p,sort_keys=True)):
        raise ValueError('complete pinned prefix Oracle and exact typed parameters required')
    mana,pairs=reviewed_payment(r,p)
    p.update(family=FAMILY,mana=mana,hybrid_pairs=pairs,power=int(r['power']),toughness=int(r['toughness']),
             legendary='Legendary' in r['type_line'],presentation=profile['presentation'])
    chosen=d.get('variants')
    if not isinstance(chosen,list) or not chosen or len(chosen)!=len(set(chosen)) or any(v not in variants(p) for v in chosen):
        raise ValueError('distinct supported prefix variants required')
    return p


def variants(p):
    modes=['unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','blocked','pump',
           'empty_library','one_card_library','short_library','duplicates','two_hits','cleanup','ready_snapshot',
           'combat_snapshot','noncombat_player','noncombat_self']
    if p['kind'] in ('top_life','one_bottom'):modes+=['nondraw','draw_positive']
    if p['kind']=='top_life':modes+=['mana_value_zero','mana_value_two','mana_value_x','hybrid_other','hybrid_mixed']
    if p['kind']=='one_bottom':modes+=['alternate_choice','reverse_bottom']
    if p['condition']:modes+=['other_own_creature','opponent_creature','own_noncreature','remove_other']
    if p['shield']:modes+=['hexproof_tapped','hexproof_real_untap']
    if p['kind']=='land_partition':modes+=['all_lands','no_lands','land_creature','partition_activation','activation_player','activation_creature','activation_source','activation_nonland','activation_empty','activation_snapshot']
    if p['kind']=='until_lands':modes+=['fourth_land','three_lands','no_lands','land_creature']
    if p['kind']=='repeat_exile':modes+=['repeat_once','repeat_twice','cross_batch_name','short_duplicate']
    if p['kind']=='scry':modes+=['scry_bottom_all','scry_keep_all','scry_event']
    if p['kind']=='surveil':modes+=['keep_all','reverse_top','delirium_four','delirium_multitype','delirium_duplicates','opponent_delirium','delirium_exile','surveil_event']
    if 'Trample' in p['keywords']:modes+=['trample_small_block']
    if 'Flying' in p['keywords']:modes+=['ground_block']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes]


def validate_witnesses(o,rows):
    data=profiles()
    for name,pin in data['witnesses'].items():
        row=rows.get(name,{})
        if o.get(name)!=pin or row.get('kind')!='card' or row.get('handler') in (None,'NULL'):
            raise ValueError('prefix witness changed: '+name)
        if 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']:
            raise ValueError('unimplemented prefix effect witness: '+name)
    for name,profile in data['sources'].items():
        if o.get(name)!=profile['oracle'] or rows.get(name,{}).get('handler')!=profile['handler'] or 'dummy' in profile['handler']:
            raise ValueError('prefix source changed: '+name)


def library(p,mode):
    kind=p['kind'];n=p['window']
    if kind=='top_life':top=['Fireball'] if mode=='mana_value_x' else ['Ornithopter'] if mode=='mana_value_zero' else ['Elvish Visionary'] if mode=='mana_value_two' else ['Reach Through Mists']
    elif kind=='one_bottom':top=['Forest','Elvish Visionary','Lightning Bolt','Ornithopter'][:n]
    elif kind=='land_partition':top=['Forest','Elvish Visionary','Plains']
    elif kind=='until_lands':top=['Forest','Lightning Bolt','Plains','Elvish Visionary','Island','Sol Ring','Mountain','Swamp']
    elif kind=='repeat_exile':top=['Forest','Plains','Island','Mountain']
    elif kind=='scry':top=['Forest','Lightning Bolt']
    else:top=['Elvish Visionary','Plains','Ornithopter']
    if mode=='all_lands':top=['Forest','Plains','Island']
    if mode=='no_lands':top=['Lightning Bolt','Elvish Visionary','Sol Ring','Ornithopter','Giant Growth']
    if mode=='land_creature':top=['Dryad Arbor']+top[1:]
    if mode=='partition_activation':top=['Dryad Arbor','Elvish Visionary','Sol Ring']
    if mode=='three_lands':return ['Lightning Bolt','Forest','Plains','Sol Ring','Island']
    if mode=='fourth_land':return ['Forest','Lightning Bolt','Plains','Island','Mountain']
    if mode=='no_lands' and kind=='until_lands':return top
    if mode in ('repeat_once','cross_batch_name'):top=['Plains','Plains','Forest','Island','Forest','Lightning Bolt','Ornithopter','Sol Ring']
    if mode=='repeat_twice':top=['Forest','Forest','Island','Mountain','Plains','Plains','Island','Sol Ring','Swamp','Lightning Bolt','Ornithopter','Giant Growth']
    if mode=='short_duplicate':return ['Forest','Forest']
    if mode=='empty_library':return []
    if mode=='one_card_library':return top[:1]
    if mode=='short_library':return top[:max(0,n-1)]
    if mode=='duplicates':top=['Forest']*n
    return top+list(TAIL)


class PrefixBoard(ReturnBoard):
    def __init__(self,name,csvid,d,p,variant,rows,o):
        from recipes import card,step,observe,cost_from_oracle
        self.card=card;self.step=step;self.observe=observe
        self.cost=lambda record:[0]*6 if record.get('mana_cost')=='{0}' else cost_from_oracle(record)
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.rows=rows;self.oracle=o
        self.mode,owner=variant.rsplit('_',1);self.owner=int(owner);self.requested=variant
        self.e=dict(draw=0,discard=0,mill=False,self_counters=0);self.keys=set(p['keywords'])
        self.steps=[];self.setup=[];self.entities={};self.tokens=[];self.bound=set();self.life=[20,20]
        self.zones=[{z:[] for z in ('hand','graveyard','library','exile')} for _ in (0,1)]
        self.pools=[[10]*6,[10]*6];self.mana=[60,60];self.discard_nondeterministic=False
        self.boundary=self.mode.startswith('noncombat_');self.optional=False;self.unordered=[0,0];self.bonus=0
        mode=self.mode;target=1 if p['kind'] in ('until_lands','repeat_exile') else 0
        for player in (0,1):self.zones[player].update(hand=['Forest','Plains'],graveyard=['Island'],exile=['Mountain'],library=['Forest','Plains','Island','Mountain','Swamp','Sol Ring','Lightning Bolt'])
        self.zones[target]['library']=library(p,mode)
        if p['delirium']:
            self.zones[0]['graveyard']=['Forest','Sol Ring','Giant Growth']
            if mode=='delirium_four':self.zones[0]['graveyard'].append('Elvish Visionary')
            if mode=='delirium_multitype':self.zones[0]['graveyard']=['Forest','Giant Growth','Ornithopter']
            if mode=='delirium_duplicates':self.zones[0]['graveyard']+=['Forest','Forest']
            if mode=='opponent_delirium':self.zones[1]['graveyard']=self.zones[0]['graveyard']+['Elvish Visionary'];self.zones[0]['graveyard']=['Forest']
        if mode=='surveil_event':
            self.zones[0]['library'].insert(0,'Mountain');self.zones[1]['hand']=['Forest']
        if mode=='activation_nonland':self.zones[0]['hand']=['Reach Through Mists']
        if mode=='activation_empty':self.zones[0]['hand']=[]
        if mode=='partition_activation':self.zones[0]['hand']=['Reach Through Mists','Lightning Bolt']
        for player in (0,1):
            for zone,names in self.zones[player].items():self.setup += [card(n,f'stock_{zone}_{player}_{i}',player,zone) for i,n in enumerate(names)]
        self.casting=mode in ('cast','payment','insufficient_mana','wrong_color','fresh_attack','hybrid_other','hybrid_mixed')
        self.add(name,'source',0,'hand' if self.casting else 'battlefield')
        self.add('Wall of Denial','wall',1)
        if mode=='trample_small_block':self.add('Grizzly Bears','small_blocker',1)
        if mode=='ground_block':self.add('Grizzly Bears','ground_blocker',1)
        if mode=='activation_creature':self.add('Grizzly Bears','activation_target',1)
        if mode in ('other_own_creature','remove_other'):self.add('Grizzly Bears','ally')
        if mode=='opponent_creature':self.add('Grizzly Bears','enemy_ally',1)
        if mode=='own_noncreature':self.add('Sol Ring','own_artifact')
        helpers={}
        if mode in ('pump','trample_small_block'):helpers['growth']='Giant Growth'
        if mode=='remove_other':helpers['remove']='Unsummon'
        if self.boundary:helpers['grant']='Hermetic Study'
        if mode in ('nondraw','draw_positive'):helpers['draw_observer']='Psychic Corrosion'
        if mode=='draw_positive':helpers['positive_draw']='Reach Through Mists'
        if mode=='scry_event':helpers['scry_observer']='Flamespeaker Adept'
        if mode=='surveil_event':helpers['surveil_observer']='Disinformation Campaign'
        if mode=='delirium_exile':helpers['crypt']="Tormod's Crypt"
        for ref,n in helpers.items():self.add(n,ref,0,'hand')
        if p['shield']:
            self.add('Giant Growth','own_probe',0,'hand');self.add('Giant Growth','enemy_probe',1,'hand')
        self.mana_cost=list(p['mana'])
        if mode in ('hybrid_other','hybrid_mixed'):
            choices=[pair[1] if mode=='hybrid_other' or i%2 else pair[0] for i,pair in enumerate(p['hybrid_pairs'])]
            self.mana_cost,_=reviewed_payment(o[name],dict(payment_colors=choices))
        if mode in ('payment','hybrid_other','hybrid_mixed'):self.pools[0]=list(self.mana_cost)
        if mode=='insufficient_mana':self.pools[0]=[0]*6
        if mode=='wrong_color':self.pools[0]=[60,0,0,0,0,0]
        if self.owner==1 and p['hybrid_pairs'] and mode in ('cast','fresh_attack'):
            # A legal AI may choose either affordable hybrid color. Restrict
            # this cast fixture to one printed route, rather than prescribe
            # an arbitrary AI spend from a fully multicolored pool.
            for pair,chosen in zip(p['hybrid_pairs'],p['payment_colors']):
                for color in pair:
                    if color!=chosen:self.pools[0][{'B':1,'U':2,'G':3,'R':4,'W':5}[color]]=0
        self.emit('turn',player=0,value=0);self.emit('discard_policy',value=1);self.emit('observe_search_filters',value=1)
        for pl in (0,1):self.set_pool(pl,self.pools[pl])
        self.sync_delirium();self.ready=len(self.steps)

    def sync_delirium(self):
        if not self.p['delirium']:return
        types=set()
        for n in self.zones[0]['graveyard']:types.update(set(self.oracle[n]['type_line'].split(' — ')[0].split())&{'Artifact','Battle','Creature','Enchantment','Instant','Kindred','Land','Planeswalker','Sorcery'})
        bonus=2 if len(types)>=4 else 0;e=self.entities['source'];e['power']+=bonus-self.bonus;e['toughness']+=bonus-self.bonus;self.bonus=bonus

    def spell(self,ref,name,target=None,entity=False):
        super().spell(ref,name,target,entity);self.sync_delirium()

    def cast_source(self):
        if self.mode in ('insufficient_mana','wrong_color'):
            self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1);return
        if self.p['hybrid_pairs']:
            from itertools import product
            from hybrid_creature_recipes import available
            vectors={tuple(reviewed_payment(self.oracle[self.name],dict(payment_colors=list(colors)))[0]) for colors in product(*self.p['hybrid_pairs'])}
            if self.owner==0 and sum(available(self.pools[0],v) for v in vectors)>1:self.emit('choose_payment',player=0,value=sum(n<<(4*c) for c,n in enumerate(self.mana_cost)))
            self.emit('cast',ref='source',value=1);self.emit('assert_card',ref='source',property='in_hand',value=0)
            # The chosen reviewed vector is the actual printed hybrid payment.
            self.pools[0]=[a-b for a,b in zip(self.pools[0],self.mana_cost)];self.mana[0]=sum(self.pools[0]);self.emit('assert_mana_total',player=0,value=self.mana[0])
            self.emit('resolve');self.move('source','battlefield')
        else:self.spell('source',self.name,entity=True)

    def draw(self,player,count):
        super().draw(player,count)
        if 'draw_observer' in self.entities and self.entities['draw_observer']['zone']=='battlefield' and player==0:
            for _ in range(count):
                n=min(2,len(self.zones[1]['library']));self.zones[1]['graveyard']+=self.zones[1]['library'][:n];del self.zones[1]['library'][:n]
        self.sync_delirium()

    def prepare_observers(self):
        for ref in ('draw_observer','scry_observer','surveil_observer','crypt'):
            if ref not in self.entities:continue
            self.spell(ref,self.entities[ref]['name'],entity=True)
            if ref=='surveil_observer':self.draw(0,1);self.discard(1,1)

    def reorder_top(self,deck,n,reverse=False):
        if n<=0:return
        if self.owner==0:
            if n==1:self.emit('choose_zone',player=0,value=-1)
            else:
                if reverse:
                    for name in deck[1:n]:self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid'])
                    deck[:n]=list(reversed(deck[:n]))
                self.emit('choose_zone',player=0,value=-1)
        elif n>1:self.unordered[0]=max(self.unordered[0],n)

    def prefix_effect(self,amount,combat=True):
        if amount<=0 or not combat:return
        p=self.p;kind=p['kind'];mode=self.mode
        if p['condition'] and any(ref!='source' and e['player']==0 and e['zone']=='battlefield' and e['creature'] for ref,e in self.entities.items()):return
        player=1 if kind in ('until_lands','repeat_exile') else 0;deck=self.zones[player]['library'];n=min(p['window'],len(deck))
        if kind=='top_life':
            if not n:return
            if self.owner==0:self.emit('choose_dialog',player=0,value=0)
            chosen=deck.pop(0);self.zones[0]['hand'].append(chosen);self.life[0]+=int(self.oracle[chosen]['cmc']);return
        if kind=='one_bottom':
            if not n:return
            window=deck[:n];chosen=window[-1] if mode=='alternate_choice' else window[0];self.emit('choose_zone_card',player=0,value=self.rows[chosen]['csvid']);window.remove(chosen)
            rest=list(reversed(window)) if mode=='reverse_bottom' else window
            for name in rest[:-1]:self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid'])
            del deck[:n];deck+=rest;self.zones[0]['hand'].append(chosen);return
        if kind=='land_partition':
            if not n:return
            self.emit('choose_zone',player=0,value=-1);self.emit('choose_zone',player=1,value=-1)
            window=deck[:n];del deck[:n]
            for name in window:self.zones[0]['hand' if 'Land' in self.oracle[name]['type_line'].split(' — ')[0].split() else 'graveyard'].append(name)
        elif kind=='until_lands':
            size=0;lands=0
            for name in deck:
                size+=1;lands+='Land' in self.oracle[name]['type_line'].split(' — ')[0].split()
                if lands==4:break
            self.emit('choose_zone',player=0,value=-1) # Actual helper's informational HUMAN display, fixed after mirroring.
            self.zones[1]['graveyard']+=deck[:size];del deck[:size]
        elif kind=='repeat_exile':
            size=0
            while size<len(deck):
                batch=deck[size:size+4];size+=len(batch)
                if len(set(batch))==len(batch):break
            if size:self.emit('choose_zone',player=0,value=-1);self.emit('choose_zone',player=1,value=-1)
            self.zones[1]['exile']+=deck[:size];del deck[:size]
        elif kind=='scry':
            self.emit('record_library',player=0)
            window=deck[:n]
            if self.owner==0:
                chosen=[] if mode=='scry_keep_all' else list(window) if mode=='scry_bottom_all' else window[-1:]
                for name in chosen:self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid']);window.remove(name)
                if window:self.emit('choose_zone',player=0,value=-1)
            else:
                # Existing AI menu initially offers nonlands when it has <4
                # lands. Selecting these is a legal Oracle subset, not an
                # assertion that an arbitrary AI must choose this policy.
                chosen=[] if mode=='scry_keep_all' else [x for x in window if 'Land' not in self.oracle[x]['type_line']]
                for name in chosen:self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid']);window.remove(name)
                if window:self.emit('choose_optional_zone',player=0,value=self.rows[window[0]]['csvid'])
            del deck[:n];deck[:0]=window;deck+=chosen
            if len(window)>1:self.reorder_top(deck,len(window))
            if 'scry_observer' in self.entities:self.entities['scry_observer']['boost']+=2 # P/T boost is asymmetric; repaired in receipts.
        elif kind=='surveil':
            chosen=[] if mode in ('keep_all','reverse_top') else list(deck[:min(2,n)])
            for name in chosen:self.emit('choose_zone_card',player=0,value=self.rows[name]['csvid']);deck.remove(name);self.zones[0]['graveyard'].append(name)
            remaining=n-len(chosen)
            if remaining:self.emit('choose_optional_zone',player=0,value=self.rows[deck[0]]['csvid'])
            if remaining>1:self.reorder_top(deck,remaining,mode=='reverse_top')
            if 'surveil_observer' in self.entities and self.entities['surveil_observer']['zone']=='battlefield':self.move('surveil_observer','hand')
        self.sync_delirium()

    def receipts(self):
        if self.mode=='remove_other':
            # A missing genuine trigger leaves its planned selector inputs
            # unused. Observe/drain that fact before the inherited strict
            # empty-plan receipt; retain all Oracle card/order assertions.
            self.emit('assert_unused_choices',value=0)
        start=len(self.steps);super().receipts()
        self.steps[start:]=[s for s in self.steps[start:] if not(s['op']=='assert_zone_card' and s['zone']=='library' and s['index']<self.unordered[s['player']])]
        if 'scry_observer' in self.entities:
            # Flamespeaker gains +2/+0, not the symmetric temporary Growth
            # field used by the generic creature receipt helper.
            for s in self.steps[start:]:
                if s['op']=='assert_card' and s.get('ref')=='scry_observer' and s['property']=='toughness':s['value']-=self.entities['scry_observer']['boost']

    def combat(self):
        mode=self.mode;self.emit('begin_attackers');legal=mode!='fresh_attack';self.emit('attack',ref='source',value=int(legal))
        if legal:self.entities['source']['tapped']=True
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers');blocked=mode in ('blocked','trample_small_block');ref='small_blocker' if mode=='trample_small_block' else 'wall'
        if blocked:self.emit('block',ref=ref,other='source',value=1)
        if mode=='ground_block':self.emit('block',ref='ground_blocker',other='source',value=0)
        self.emit('advance_combat');amount=self.power('source') if legal else 0
        if blocked:
            e=self.entities[ref];dealt=min(amount,e['toughness']) if 'Trample' in self.keys else amount;excess=amount-dealt if 'Trample' in self.keys else 0
            e['damage']+=dealt;self.entities['source']['damage']+=e['power']
            if e['damage']>=e['toughness']:self.move(ref,'graveyard')
            amount=excess
        self.life[1]-=amount;self.prefix_effect(amount);self.emit('combat_damage');self.emit('resolve');self.emit('end_combat')
        if amount and self.p['kind']=='scry':self.emit('assert_scry_result',player=0,value=2)
        return cut

    def probe_shield(self):
        self.emit('refresh');self.emit('can_target_creature',ref='own_probe',other='source',value=1)
        denied=self.p['shield']=='always' or self.p['shield']=='untapped' and not self.entities['source']['tapped']
        self.emit('can_target_creature',ref='enemy_probe',other='source',value=0 if denied else 1)

    def land_activation(self):
        legal=self.mode not in ('activation_nonland','activation_empty');self.emit('can_activate',ref='source',value=int(legal))
        if not legal:return len(self.steps)
        land='Dryad Arbor' if self.mode=='partition_activation' else 'Forest'
        self.emit('require_can_activate',ref='source',value=1)
        self.emit('choose_zone_card',player=0,value=self.rows[land]['csvid'])
        target='source' if self.mode=='activation_source' else 'activation_target' if self.mode=='activation_creature' else None
        if target:self.emit('choose_target',player=0,ref=target)
        else:self.emit('choose_player',player=0,value=1)
        self.emit('activate',ref='source',value=1);self.zones[0]['hand'].remove(land);self.zones[0]['graveyard'].append(land);cut=len(self.steps)
        self.emit('assert_zone_multiplicity',player=0,zone='graveyard',index=self.rows[land]['csvid'],value=self.zones[0]['graveyard'].count(land))
        if target:
            self.entities[target]['damage']+=3
            if self.entities[target]['damage']>=self.entities[target]['toughness']:self.move(target,'graveyard')
        else:self.life[1]-=3
        self.emit('resolve');return cut

    def run(self):
        mode=self.mode;cut=self.ready
        if self.casting:self.cast_source()
        if not self.casting:self.prepare_observers()
        if self.entities['source']['zone']=='battlefield':self.emit('assert_card',ref='source',property='power',value=self.power('source'))
        if self.p['shield'] and self.entities['source']['zone']=='battlefield':self.probe_shield()
        if mode.startswith('activation'):cut=self.land_activation()
        elif self.boundary:
            self.spell('grant','Hermetic Study','source',True);cut=len(self.steps);target=0 if mode=='noncombat_self' else 1
            self.emit('choose_player',player=0,value=target);self.emit('activate',ref='grant',value=1);self.entities['source']['tapped']=True;self.life[target]-=1
            self.emit('assert_card',ref='source',property='tapped',value=1);self.emit('assert_card',ref='grant',property='tapped',value=0);self.emit('resolve')
        elif not self.casting or mode=='fresh_attack':
            if mode=='remove_other':
                self.spell('remove','Unsummon','ally');self.move('ally','hand')
                self.emit('assert_card',ref='ally',property='in_play',value=0)
                self.emit('assert_zone',player=0,zone='battlefield',value=1)
                self.emit('assert_zone_multiplicity',player=0,zone='hand',index=self.rows['Grizzly Bears']['csvid'],value=1)
            if mode in ('pump','trample_small_block'):self.spell('growth','Giant Growth','source');self.entities['source']['boost']+=3
            cut=self.combat()
            if mode=='partition_activation':self.receipts();self.land_activation()
            if self.p['shield']:self.probe_shield()
            if mode=='two_hits':self.receipts();self.next_own_turn();self.combat()
            if mode=='hexproof_real_untap':self.receipts();self.next_own_turn();self.probe_shield()
            if mode=='draw_positive':self.spell('positive_draw','Reach Through Mists');self.draw(0,1)
            if mode=='delirium_exile':
                self.emit('choose_player',player=0,value=0);self.emit('activate',ref='crypt',value=1);self.move('crypt','graveyard');self.zones[0]['exile']+=self.zones[0]['graveyard'];self.zones[0]['graveyard']=[];self.emit('resolve');self.sync_delirium()
            if mode in ('pump','trample_small_block','cleanup','scry_event'):
                self.receipts()
                for pl in (0,1):self.set_pool(pl,[0]*6)
                self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','activation_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut;replay=steps[point:]
            steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=['Information visibility (reveal versus look) is recognized in full Oracle but not inferred from matching zone movement.',
              'No invented damage-trigger announcement priority window; pending target loss and intervening-if changes remain separate observations.',
              'Planeswalker branch of any-target land-discard activation remains a separate reviewed witness observation.' if self.p['land_damage'] else 'Multiplayer, prevention/replacements and extra combat steps remain separate observations.']
        case=dict(name=f'CombatLibraryPrefixRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        if self.p['kind']=='until_lands':
            for s in case['steps']:
                if s['op']=='choose_zone':s['player']=0 # mind_funeral_effect explicitly displays to HUMAN.
        return case


def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in variants(p) for v in selected):raise ValueError('distinct supported prefix variants required')
    return [PrefixBoard(name,csvid,d,p,v,rows,o).run() for v in selected]
