"""Oracle-typed destruction from real declarations, damage and end combat."""
import copy
import hashlib
import json
import re
from pathlib import Path

from return_saboteur_trigger_recipes import ReturnBoard
from combat_value_trigger_recipes import _mirror
from grant_recipes import LIBRARY

FAMILY='block_damage_destruction'
PROFILE_PATH=Path(__file__).parent/'data/block_damage_destruction_wave8_profiles.json'


def profiles():return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(r):
    if (not isinstance(r,dict) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line','') or any(not re.fullmatch(r'[0-8]',r.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('destruction requires fixed normal printed characteristics')
    aliases={'this creature',r['name'],r['name'].split(',')[0]}
    source='(?:'+'|'.join(re.escape(x) for x in sorted(aliases))+')'
    keys=[];lines=[];artifact_power=False;snake_bonus=False;draw=False
    for line in r['oracle_text'].splitlines():
        if line=='First strike':keys.append(line);continue
        if re.fullmatch('(?:This creature|'+source+r') gets \+1/\+0 for each artifact you control\.',line):artifact_power=True;continue
        if line=='Other Snake creatures you control get +1/+0.':snake_bonus=True;continue
        if re.fullmatch(r'Whenever '+source+r' deals combat damage to a player, you may draw a card\.',line):draw=True;continue
        lines.append(line)
    body='\n'.join(lines)
    trigger=None;role=None;scope=None;victims='other';timing='end_combat';predicate='any';noregen=False;gain=False
    if re.fullmatch(r'When '+source+r' blocks a creature, destroy (?:both creatures|that creature and this creature)\.',body):
        trigger='block';role='defending';victims='both';timing='immediate'
    m=re.fullmatch(r'Whenever '+source+r' blocks or becomes blocked by a creature(?: with power 1 or less)?, destroy (this|that) creature( at end of combat)?(?:\. You gain life equal to that creature\x27s toughness)?\.',body)
    if m:
        trigger='block';role='both';victims='self' if m[1]=='this' else 'other';timing='end_combat' if m[2] else 'immediate'
        predicate='power_le1' if 'with power 1 or less' in body else 'any';gain="You gain life equal to that creature's toughness" in body
    if re.fullmatch(r'Whenever '+source+r' blocks a creature, destroy that creature at end of combat\.\nWhenever '+source+r' becomes blocked by a non-Wall creature, destroy that creature at end of combat\.',body):
        trigger='block';role='split';predicate='attacking_nonwall';scope=None
    m=re.fullmatch(r'Whenever '+source+r' blocks or becomes blocked by a (green or white|nonblack|non-Wall) creature, destroy that creature at end of combat\.',body)
    if m:trigger='block';role='both';predicate={'green or white':'green_white','nonblack':'nonblack','non-Wall':'nonwall'}[m[1]]
    m=re.fullmatch(r'Whenever ('+source+r'|a Warrior you control|a Sliver) deals (combat )?damage to a creature, destroy that creature( at end of combat)?\.( (?:It|The creature) can\x27t be regenerated\.)?',body)
    if m:
        trigger='damage';role='source' if m[1] in aliases else 'own_warrior' if m[1]=='a Warrior you control' else 'all_slivers'
        scope='combat' if m[2] else 'any';timing='end_combat' if m[3] else 'immediate';noregen=bool(m[4])
    if trigger is None or set(keys)!=set(r.get('keywords',[])) or len(keys)!=len(set(keys)):
        raise ValueError('complete supported block/damage clauses and keyword metadata required')
    if artifact_power and not (trigger=='damage' and role=='source') or snake_bonus and role!='own_warrior':
        raise ValueError('unreviewed companion composition')
    return dict(trigger=trigger,role=role,scope=scope,victims=victims,timing=timing,predicate=predicate,cannot_regenerate=noregen,
                toughness_life=gain,artifact_power=artifact_power,snake_bonus=snake_bonus,optional_player_draw=draw,keywords=sorted(keys))


def spec(d,r):
    from recipes import cost_from_oracle
    p=parameters(r);pin=profiles()['sources'].get(r['name'])
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or not pin
            or pin['oracle']!=r or pin['params']!=p or d.get('oracle_text')!=r['oracle_text']
            or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(p,sort_keys=True)):
        raise ValueError('whole pinned source and exact typed destruction parameters required')
    p.update(family=FAMILY,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),follow_on=pin.get('follow_on') is True)
    selected=d.get('variants')
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in variants(p) for v in selected):
        raise ValueError('distinct supported destruction variants required')
    return p


def variants(p):
    modes=['attack_blocked','defend','unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack',
           'pump','indestructible_victim','source_indestructible','ready_snapshot','declared_snapshot','damage_snapshot',
           'noncombat_creature','noncombat_player']
    if p['timing']=='end_combat':modes+=['source_removed']
    if p['victims']!='self':modes+=['regenerate_victim']
    if p['predicate']=='power_le1':modes+=['one_power','two_power']
    if p['predicate'] in ('green_white','nonblack'):
        modes+=['green_creature','white_creature','black_creature','colorless_creature','multicolor_black','multicolor_white']
    if p['role']=='split' or p['predicate'] in ('nonwall','attacking_nonwall'):modes+=['wall_attack','wall_defend']
    if p['toughness_life']:modes+=['toughness_gain']
    if p['artifact_power']:modes+=['no_artifacts','opponent_artifacts','creature_artifact','two_artifacts','artifact_bounce']
    if p['role'] in ('own_warrior','all_slivers'):modes+=['ally_hit','enemy_hit','wrong_type_hit','ally_noncombat']
    if p['snake_bonus']:modes+=['snake_bonus','opponent_snake']
    if p['trigger']=='damage':modes+=['first_strike']
    if 'First strike' in p['keywords']:modes+=['intrinsic_first_strike']
    if p['optional_player_draw']:modes+=['draw_declined']
    return [v+'_'+str(owner) for owner in (0,1) for v in modes if not(v=='draw_declined' and owner==1)]


def validate_witnesses(o,rows):
    data=profiles()
    for name,pin in data['witnesses'].items():
        row=rows.get(name,{})
        if o.get(name)!=pin or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0'):
            raise ValueError('destruction witness changed: '+name)
        if 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']:
            raise ValueError('unimplemented effect witness: '+name)
    for name,pin in data['sources'].items():
        if o.get(name)!=pin['oracle'] or rows.get(name,{}).get('handler')!=pin['handler'] or 'dummy' in pin['handler']:
            raise ValueError('destruction primary changed: '+name)
        source=pin['compiled_source'];root=Path(__file__).resolve().parents[4]
        if hashlib.sha256((root/source['path']).read_bytes()).hexdigest()!=source['sha256'] or '/jatill.' in source['path']:
            raise ValueError('compiled destruction source changed: '+name)


class DestructionBoard(ReturnBoard):
    def __init__(self,name,csvid,d,p,variant,rows,o):
        from recipes import card,step,observe,cost_from_oracle
        self.card=card;self.step=step;self.observe=observe;self.cost=cost_from_oracle
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.rows=rows;self.oracle=o
        self.mode,owner=variant.rsplit('_',1);self.owner=int(owner);self.requested=variant
        self.steps=[];self.setup=[];self.entities={};self.tokens=[];self.bound=set();self.keys=set(p['keywords'])
        self.e=dict(draw=0,discard=0,mill=False,self_counters=0);self.optional=False;self.discard_nondeterministic=False
        self.life=[20,20];self.mana=[60,60];self.pools=[[10]*6,[10]*6]
        self.zones=[{z:[] for z in ('hand','graveyard','library','exile')} for _ in (0,1)]
        mode=self.mode;self.boundary=mode.startswith('noncombat') or mode=='ally_noncombat';self.delayed=[];self.regen_used=False
        self.defending=mode in ('defend','wall_defend') or p['role']=='defending' and mode not in ('attack_blocked','unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','noncombat_creature','noncombat_player')
        self.actor=1 if self.defending or mode=='enemy_hit' else 0
        for pl in (0,1):
            self.zones[pl].update(hand=['Forest','Plains'],graveyard=['Island'],exile=['Mountain'],library=list(LIBRARY))
            for zone,names in self.zones[pl].items():self.setup += [card(n,f'stock_{zone}_{pl}_{i}',pl,zone) for i,n in enumerate(names)]
        self.casting=mode in ('cast','payment','insufficient_mana','wrong_color','fresh_attack')
        self.add(name,'source',0,'hand' if self.casting else 'battlefield')
        # Rock's4power must not naturally kill the positive delayed witness.
        victim='Mephitic Ooze' if p['predicate']=='nonwall' else 'Lagonna-Band Trailblazer'
        if mode=='wall_attack':victim='Wall of Denial'
        if mode=='wall_defend':victim='Mistform Ultimus'
        if mode=='indestructible_victim':victim='Darksteel Myr'
        if mode=='one_power':victim='Llanowar Elves'
        if mode in ('two_power','toughness_gain'):victim='Grizzly Bears'
        if mode=='regenerate_victim':
            victim='Cudgel Troll' if p['predicate']=='nonwall' else 'River Boa' if self.defending or p['role']=='split' else 'Wall of Brambles'
        if mode in ('first_strike','intrinsic_first_strike'):victim='Abyssal Specter'
        colors={'green_creature':'Wall of Roots','white_creature':'Lagonna-Band Trailblazer','black_creature':'Wall of Bone',
                'colorless_creature':'Wall of Tanglecord','multicolor_black':'Dreg Mangler','multicolor_white':'Wall of Denial'}
        if p['predicate'] in ('green_white','nonblack') and mode in colors:victim=colors[mode]
        self.add(victim,'victim',1)
        if mode in ('ally_hit','enemy_hit','wrong_type_hit','ally_noncombat','snake_bonus','opponent_snake'):
            ally='Grizzly Bears' if mode=='wrong_type_hit' else 'Changeling Sentinel'
            self.add(ally,'ally',1 if mode in ('enemy_hit','opponent_snake') else 0)
            self.add('Lagonna-Band Trailblazer' if mode=='ally_noncombat' else 'Wall of Denial','team_victim',0 if mode=='enemy_hit' else 1)
        if p['artifact_power']:
            if mode not in ('no_artifacts','opponent_artifacts'):self.add('Ornithopter' if mode=='creature_artifact' else 'Sol Ring','artifact')
            if mode in ('opponent_artifacts','two_artifacts'):self.add('Sol Ring','artifact2',1 if mode=='opponent_artifacts' else 0)
        if self.boundary:self.add('Hermetic Study','grant',0,'hand')
        if mode=='pump':self.add('Giant Growth','helper',0,'hand')
        if mode=='source_indestructible':self.add('Withstand Death','helper',0,'hand')
        if mode=='source_removed':self.add('Unsummon','helper',0,'hand')
        if mode=='artifact_bounce':self.add('Boomerang','helper',0,'hand')
        if mode=='toughness_gain':self.add('Giant Growth','victim_spell',0,'hand')
        self.victim_aura=mode=='regenerate_victim' or mode=='wall_defend' and p['predicate']=='nonwall'
        if self.victim_aura:self.add('Holy Strength','victim_spell',1,'hand')
        if mode=='first_strike':self.add('Reflexes','strike_aura',0,'hand')
        if mode=='payment':self.pools[0]=list(p['mana'])
        if mode=='insufficient_mana':self.pools[0]=[0]*6
        if mode=='wrong_color':self.pools[0]=[60,0,0,0,0,0]
        self.emit('turn',player=1 if self.victim_aura else self.actor,value=0);self.emit('present_ai_dialogs',value=1);self.emit('discard_policy',value=1)
        for pl in (0,1):self.set_pool(pl,self.pools[pl])
        self.sync_stats();self.ready=len(self.steps)

    def add(self,*args,**kw):
        super().add(*args,**kw);e=self.entities[args[1]];e['base_power']=e['power'];e['base_toughness']=e['toughness']
        e['indestructible']='Indestructible' in self.oracle[e['name']].get('keywords',[])

    def subtype(self,ref,subtype):
        r=self.oracle[self.entities[ref]['name']]
        return ('Changeling' in r.get('keywords',[]) or 'is every creature type' in r.get('oracle_text','')
                or subtype in r['type_line'].split(' — ')[-1].split())

    def sync_stats(self):
        alive=self.entities['source']['zone']=='battlefield'
        count=sum(e['zone']=='battlefield' and e['player']==0 and 'Artifact' in self.oracle[e['name']]['type_line'] for e in self.entities.values())
        for ref,e in self.entities.items():
            e['power']=e['base_power']+(count if ref=='source' and self.p['artifact_power'] and alive else 0)
            if self.p['snake_bonus'] and alive and ref!='source' and e['player']==0 and self.subtype(ref,'Snake'):e['power']+=1
            e['toughness']=e['base_toughness']

    def receipts(self):
        self.sync_stats();self.emit('assert_unused_choices',value=0);super().receipts()

    def cast_other(self,ref,target,player=1):
        e=self.entities[ref];n=e['name'];self.emit('choose_target',player=player,ref=target);self.emit('cast',ref=ref,value=1)
        self.spend(self.oracle[n]['mana_cost'],player);self.emit('assert_stack',value=1);self.emit('resolve')
        self.move(ref,'battlefield' if 'Enchantment' in self.oracle[n]['type_line'] else 'graveyard')
        self.emit('assert_mana_total',player=player,value=self.mana[player])
        for color,value in enumerate(self.pools[player]):self.emit('assert_mana',player=player,color=color,value=value)
        if n=='Holy Strength':self.entities[target]['base_power']+=1;self.entities[target]['base_toughness']+=2
        else:self.entities[target]['boost']+=3
        self.sync_stats()

    def kill(self,ref,allow_regen=True):
        e=self.entities[ref]
        if e['zone']!='battlefield' or e['indestructible']:return
        if ref=='victim' and self.mode=='regenerate_victim' and allow_regen:
            self.spend('{G}',e['player']);e.update(tapped=True,damage=0);self.regen_used=True;return
        self.move(ref,'graveyard')
        attached=['grant','strike_aura'] if ref=='source' else ['victim_spell'] if ref=='victim' else []
        for aura in attached:
            if aura in self.entities and self.entities[aura]['zone']=='battlefield':self.move(aura,'graveyard')
        self.sync_stats()

    def condition(self,other):
        e=self.entities[other];r=self.oracle[e['name']];pred=self.p['predicate']
        if pred=='power_le1':return self.power(other)<=1
        if pred=='attacking_nonwall':return self.defending or not self.subtype(other,'Wall')
        if pred=='nonwall':return not self.subtype(other,'Wall')
        if pred=='green_white':return bool(set(r.get('colors',[]))&{'G','W'})
        if pred=='nonblack':return 'B' not in r.get('colors',[])
        return True

    def destroy_effect(self,other):
        refs=['source',other] if self.p['victims']=='both' else ['source'] if self.p['victims']=='self' else [other]
        if self.p['toughness_life']:self.life[0]+=self.entities[other]['toughness']+self.entities[other]['boost']+self.entities[other]['counters']
        if self.p['timing']=='end_combat':self.delayed+=refs
        else:
            for ref in refs:self.kill(ref,not self.p['cannot_regenerate'])

    def damage_effect(self,ref,target,amount,combat=True):
        p=self.p
        if p['trigger']!='damage' or amount<=0 or not combat and p['scope']=='combat':return
        qualifies=ref=='source' if p['role']=='source' else self.entities[ref]['player']==0 and self.subtype(ref,'Warrior') if p['role']=='own_warrior' else self.subtype(ref,'Sliver')
        if qualifies:self.destroy_effect(target)

    def regen_choice(self,stage):
        if self.mode!='regenerate_victim':return
        desired='end' if self.p['timing']=='end_combat' else 'declaration' if self.p['trigger']=='block' else 'damage'
        if stage==desired:self.emit('choose_regeneration',ref='victim')

    def combat(self):
        mode=self.mode;actor=self.actor;team=mode in ('ally_hit','enemy_hit','wrong_type_hit')
        attacker='ally' if team else 'victim' if self.defending else 'source'
        blocker='team_victim' if team else 'source' if self.defending else 'victim'
        self.emit('begin_attackers');legal=mode!='fresh_attack';self.emit('attack',ref=attacker,value=int(legal))
        if legal:self.entities[attacker]['tapped']='Vigilance' not in self.oracle[self.entities[attacker]['name']].get('keywords',[])
        self.emit('finish_attackers');self.emit('begin_blockers');blocked=legal and mode not in ('unblocked','draw_declined')
        if blocked:self.emit('block',ref=blocker,other=attacker,value=1)
        self.regen_choice('declaration');self.emit('advance_combat')
        if blocked and self.p['trigger']=='block' and not team and (self.p['role']!='defending' or self.defending) and self.condition('victim'):self.destroy_effect('victim')
        if self.mode=='regenerate_victim' and self.p['cannot_regenerate'] and self.p['trigger']=='block':self.emit('assert_unused_choices',value=1)
        self.receipts();declared=len(self.steps)
        self.regen_choice('damage')
        if blocked and not self.regen_used and all(self.entities[r]['zone']=='battlefield' for r in (attacker,blocker)):
            a=self.power(attacker);b=self.power(blocker)
            first='source' in (attacker,blocker) and ('First strike' in self.keys)
            if first:
                other=blocker if attacker=='source' else attacker;damage=self.power('source')
                self.entities[other]['damage']+=damage;self.damage_effect('source',other,damage)
                if self.entities[other]['zone']=='battlefield' and self.entities[other]['damage']>=self.entities[other]['toughness']+self.entities[other]['boost']:self.kill(other)
                if self.entities[other]['zone']=='battlefield' and not self.regen_used:self.entities['source']['damage']+=self.power(other)
            else:
                self.entities[blocker]['damage']+=a;self.entities[attacker]['damage']+=b
                self.damage_effect(attacker,blocker,a);self.damage_effect(blocker,attacker,b)
            for ref in (attacker,blocker):
                e=self.entities[ref]
                if e['zone']=='battlefield' and e['damage']>=e['toughness']+e['boost']+e['counters']:self.kill(ref)
        elif legal and not blocked:
            amount=self.power(attacker);self.life[1-actor]-=amount
            if self.p['optional_player_draw'] and amount and mode!='draw_declined':
                if self.owner==0:self.emit('choose_trigger_source',player=0,ref='source')
                self.draw(0,1)
        self.emit('combat_damage');self.emit('resolve')
        if self.mode=='regenerate_victim' and self.p['cannot_regenerate'] and self.p['trigger']=='damage':self.emit('assert_unused_choices',value=1)
        self.receipts();damaged=len(self.steps)
        if mode=='source_removed' and self.entities['source']['zone']=='battlefield':self.spell('helper','Unsummon','source');self.move('source','hand');self.sync_stats()
        self.regen_choice('end');self.emit('end_combat')
        for ref in self.delayed:self.kill(ref,not self.p['cannot_regenerate'])
        self.delayed=[]
        if self.mode=='regenerate_victim' and self.p['cannot_regenerate'] and self.p['timing']=='end_combat':self.emit('assert_unused_choices',value=1)
        return declared,damaged

    def noncombat(self):
        ref='ally' if self.mode=='ally_noncombat' else 'source'
        self.spell('grant','Hermetic Study',ref,True);self.emit('can_activate',ref='grant',value=1)
        target=None if self.mode=='noncombat_player' else 'team_victim' if ref=='ally' else 'victim'
        if target:self.emit('choose_target',player=0,ref=target)
        else:self.emit('choose_player',player=0,value=1)
        self.emit('activate',ref='grant',value=1);self.emit('assert_stack',value=1);self.entities[ref]['tapped']=True
        self.emit('assert_card',ref=ref,property='tapped',value=1);self.emit('assert_card',ref='grant',property='tapped',value=0)
        if target:self.entities[target]['damage']+=1;self.damage_effect(ref,target,1,False)
        else:self.life[1]-=1
        self.emit('resolve');self.receipts()
        if self.delayed:
            # An actual empty combat reaches the next genuine end-combat.
            for op in ('begin_attackers','finish_attackers','begin_blockers','advance_combat','combat_damage'):self.emit(op)
            self.emit('end_combat')
            for victim in self.delayed:self.kill(victim)
            self.delayed=[]

    def run(self):
        mode=self.mode;declared=damaged=self.ready
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if self.boundary:self.noncombat()
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','source_indestructible'):
                n='Giant Growth' if mode=='pump' else 'Withstand Death';self.spell('helper',n,'source')
                if mode=='pump':self.entities['source']['boost']+=3
                else:self.entities['source']['indestructible']=True
            if mode=='toughness_gain' or self.victim_aura:
                self.cast_other('victim_spell','victim',0 if mode=='toughness_gain' else 1)
                if self.victim_aura and self.actor==0:
                    # Aura was really cast on its controller's prior turn.
                    # Clear floated pools before the actual turn/untap handoff.
                    for pl in (0,1):self.set_pool(pl,[0]*6)
                    self.emit('advance_turn');self.emit('untap_step',player=0);self.emit('turn',player=0,value=0)
                    for pl in (0,1):self.set_pool(pl,[10]*6)
            if mode=='first_strike':self.spell('strike_aura','Reflexes','source',True);self.keys.add('First strike')
            if mode=='artifact_bounce':self.spell('helper','Boomerang','artifact');self.move('artifact','hand');self.sync_stats()
            declared,damaged=self.combat()
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','declared_snapshot','damage_snapshot'):
            cut=self.ready if mode=='ready_snapshot' else declared if mode=='declared_snapshot' else damaged
            replay=steps[cut:];steps=copy.deepcopy(steps[:cut])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=['No fabricated pending-trigger priority window or injected damage/legacy/history. Multiplayer, prevention/replacement and simultaneous many-source ordering remain separate.',
              'AI voluntary optional draw decline is not claimed without an actual UI path. First-strike and optional-draw follow-on source enrollment remains gated by pilot acceptance.']
        if self.p.get('follow_on'):
            gaps[1]='AI voluntary optional draw decline is not claimed without an actual UI path; actual human decline and both-controller acceptance are covered for the optional player-draw source. Intrinsic first-strike ordering has its own real combat control.'
        case=dict(name=f'BlockDamageDestructionRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


def expand(name,csvid,d,p,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in variants(p) for v in selected):raise ValueError('distinct supported destruction variants required')
    return [DestructionBoard(name,csvid,d,p,v,rows,oracle).run() for v in selected]
