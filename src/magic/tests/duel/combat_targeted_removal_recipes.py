"""Whole-Oracle targeted saboteurs through real combat and removal actions."""
import copy
import json
import re
from pathlib import Path

from combat_value_trigger_recipes import _mirror
from return_saboteur_trigger_recipes import ReturnBoard
from grant_recipes import LIBRARY

FAMILY = 'combat_targeted_removal'
PROFILE_PATH = Path(__file__).parent/'data/combat_targeted_removal_wave8_profiles.json'


def profiles(): return json.loads(PROFILE_PATH.read_text(encoding='utf8'))


def parameters(r):
    if (not isinstance(r,dict) or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face')
            or 'Creature' not in r.get('type_line','')
            or any(not re.fullmatch(r'[1-8]',r.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('targeted removal requires fixed normal creature characteristics')
    alias='(?:'+'|'.join(re.escape(n) for n in sorted({'this creature',r['name'],r['name'].split(',')[0]}))+')'
    keys=[];ninjutsu=None;maxblockers=None;trigger=None
    for line in r['oracle_text'].splitlines():
        if line=='Flying':keys.append('Flying');continue
        if line=="Shadow (This creature can block or be blocked by only creatures with shadow.)":keys.append('Shadow');continue
        if line=="Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)":keys.append('Intimidate');continue
        if line=="This creature can't be blocked by more than one creature.":maxblockers=1;continue
        m=re.fullmatch(r'Ninjutsu ((?:\{[0-9BUGRW]+\})+) \(\1, Return an unblocked attacker you control to hand: Put this card onto the battlefield from your hand tapped and attacking\.\)',line)
        if m:
            if ninjutsu:raise ValueError('duplicate ninjutsu')
            ninjutsu=m[1];keys.append('Ninjutsu');continue
        m=re.fullmatch(r'Whenever '+alias+r' deals (combat )?damage to a player, (.+)',line)
        if not m or trigger is not None:raise ValueError('additional or duplicate removal clause')
        trigger=m
    if trigger is None:raise ValueError('complete removal trigger required')
    body=trigger[2];payment=None;optional=False
    m=re.fullmatch(r'(you may )?destroy target (artifact|enchantment|artifact or enchantment|nonblack creature|creature) that player controls\.',body)
    if m:
        effect='destroy';selector=m[2].replace(' or ','_or_').replace(' ','_');optional=bool(m[1])
    elif body=='choose target creature that player controls. The player sacrifices that creature.':
        effect='targeted_sacrifice';selector='creature'
    elif body=='you may sacrifice it. If you do, destroy target creature that player controls.':
        effect='sacrifice_source_then_destroy';selector='creature';optional=True
    else:
        m=re.fullmatch(r'you may pay ((?:\{[0-9BUGRW]+\})+)\. If you do, destroy up to two target lands\.',body)
        if not m:raise ValueError('unreviewed removal effect')
        effect='pay_then_destroy_up_to_two';selector='land';payment=m[1];optional=True
    if len(keys)!=len(set(keys)) or set(keys)!=set(r.get('keywords',[])):raise ValueError('complete removal keyword metadata mismatch')
    return dict(effect=effect,selector=selector,optional=optional,scope='combat' if trigger[1] else 'any',
                payment=payment,ninjutsu=ninjutsu,maxblockers=maxblockers,keywords=sorted(keys))


def spec(d,r):
    from recipes import cost_from_oracle
    p=parameters(r);profile=profiles()['sources'].get(r['name'])
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
            or not profile or profile['oracle']!=r or profile['params']!=p or d.get('oracle_text')!=r['oracle_text']
            or json.dumps(d.get('params'),sort_keys=True)!=json.dumps(p,sort_keys=True)):
        raise ValueError('complete source pin and typed removal parameters required')
    p.update(family=FAMILY,mana=cost_from_oracle(r),power=int(r['power']),toughness=int(r['toughness']),
             legendary='Legendary' in r['type_line'],input_profile=profile['input_profile'])
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or any(v not in variants(p) for v in vs):raise ValueError('distinct supported removal variants required')
    return p


def variants(p):
    modes=['unblocked','cast','payment','insufficient_mana','wrong_color','fresh_attack','blocked',
           'pump','cleanup','no_target','wrong_type','ready_snapshot','combat_snapshot',
           'noncombat_player','noncombat_self','noncombat_snapshot','noncombat_sick']
    if p['effect']!='sacrifice_source_then_destroy':modes+=['two_hits','source_bounce']
    if p['selector']!='land':modes+=['other_controller']
    if p['selector'] in ('creature','nonblack_creature','artifact','artifact_or_enchantment'):modes+=['indestructible']
    if p['selector']=='artifact_or_enchantment':modes+=['enchantment_target']
    if p['selector'] in ('creature','nonblack_creature'):modes+=['shroud_only','owner_destination']
    if p['selector']=='nonblack_creature':modes+=['black_only','artifact_creature']
    if 'Flying' in p['keywords'] or 'Shadow' in p['keywords']:modes+=['keyword_block','contrary_block']
    if 'Intimidate' in p['keywords']:modes+=['artifact_block','color_block','contrary_block']
    if p['maxblockers']:modes+=['second_block']
    if p['ninjutsu']:modes+=['ninjutsu','ninjutsu_payment','ninjutsu_insufficient','ninjutsu_before_blocks','ninjutsu_blocked','ninjutsu_pending_snapshot']
    if p['payment']:modes+=['trigger_payment','trigger_insufficient','trigger_wrong_color','zero_targets','one_target','mixed_lands','own_lands']
    result=[v+'_'+str(owner) for owner in (0,1) for v in modes]
    if p['optional']:result+=['declined_0']
    return result


def validate_witnesses(o,rows):
    data=profiles()
    for name,pin in data['witnesses'].items():
        row=rows.get(name,{})
        if o.get(name)!=pin or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0'):
            raise ValueError('removal witness changed: '+name)
        if 'dummy' in row['handler'] and name not in data['native_characteristics_only_witnesses']:raise ValueError('unimplemented removal effect witness: '+name)
    for name,pin in data['sources'].items():
        if o.get(name)!=pin['oracle'] or rows.get(name,{}).get('handler')!=pin['handler'] or 'dummy' in pin['handler']:raise ValueError('removal source implementation changed: '+name)


class RemovalBoard(ReturnBoard):
    """Expected movement comes from Oracle; menus are presentation annotations."""
    def __init__(self,name,csvid,d,p,variant,rows,o):
        from recipes import card,step,observe,cost_from_oracle
        self.card=card;self.step=step;self.observe=observe;self.cost=cost_from_oracle
        self.name=name;self.csvid=csvid;self.d=d;self.p=p;self.rows=rows;self.oracle=o
        self.mode,owner=variant.rsplit('_',1);self.owner=int(owner);self.requested=variant
        self.steps=[];self.setup=[];self.entities={};self.tokens=[];self.bound=set()
        self.e=dict(draw=0,discard=0,mill=False,self_counters=0);self.keys=set(p['keywords'])
        self.life=[20,20];self.pools=[[10]*6,[10]*6];self.mana=[60,60]
        self.zones=[{z:[] for z in ('hand','graveyard','exile','library')} for _ in (0,1)]
        self.boundary=self.mode.startswith('noncombat_');self.optional=False;self.discard_nondeterministic=False
        mode=self.mode;selector=p['selector']
        for player in (0,1):
            self.zones[player].update(hand=['Forest','Plains'],graveyard=['Island'],exile=['Mountain'],library=list(LIBRARY))
            for zone,names in self.zones[player].items():self.setup += [card(n,f'stock_{zone}_{player}_{i}',player,zone) for i,n in enumerate(names)]
        self.casting=mode in ('cast','payment','insufficient_mana','wrong_color','fresh_attack')
        self.hand_mode=mode.startswith('ninjutsu')
        self.add(name,'source',0,'hand' if self.casting or self.hand_mode else 'battlefield', ['sick'] if mode=='noncombat_sick' else None)
        self.add('Grizzly Bears','idle')
        blocker='Dauthi Slayer' if 'Shadow' in self.keys else 'Darksteel Gargoyle' if 'Intimidate' in self.keys and mode in ('blocked','artifact_block') else 'Wall of Denial'
        if mode in ('contrary_block','second_block'):blocker='Grizzly Bears'
        if mode=='color_block':blocker='Abyssal Specter'
        if mode=='keyword_block' and 'Flying' in self.keys:blocker='Wall of Denial'
        self.add(blocker,'wall',1)
        if mode=='second_block':self.add('Grizzly Bears','second_blocker',1)
        target='Sol Ring' if selector in ('artifact','artifact_or_enchantment') else 'Fervor' if selector=='enchantment' else 'Forest' if selector=='land' else 'Grizzly Bears'
        if mode=='wrong_type':target='Grizzly Bears' if selector in ('artifact','enchantment','artifact_or_enchantment','land') else 'Sol Ring'
        if mode=='black_only':target='Abyssal Specter'
        if mode=='artifact_creature':target='Ornithopter'
        if mode=='shroud_only':target='Wall of Denial'
        if mode=='indestructible':target='Darksteel Myr'
        if mode=='enchantment_target':target='Fervor'
        target_player=0 if mode in ('other_controller','owner_destination') else 1
        if mode!='no_target':self.add(target,'victim',target_player)
        if selector=='land':
            if mode not in ('no_target','wrong_type'):self.add('Plains','victim2',1)
            if mode in ('mixed_lands','own_lands'):self.add('Island','own_land')
            if mode=='own_lands':self.add('Mountain','own_land2')
        if mode=='two_hits':self.add(target,'later_victim',1)
        if mode in ('pump','cleanup'):self.add('Giant Growth','helper',0,'hand')
        if mode=='source_bounce':self.add('Unsummon','helper',0,'hand')
        if self.boundary:self.add('Hermetic Study','grant',0,'hand')
        if mode=='owner_destination':self.add('Control Magic','theft',1,'hand')
        if mode=='payment':self.pools[0]=list(p['mana'])
        if mode in ('insufficient_mana','trigger_insufficient','ninjutsu_insufficient'):self.pools[0]=[0]*6
        if mode in ('wrong_color','trigger_wrong_color'):self.pools[0]=[60,0,0,0,0,0]
        if mode=='trigger_payment':self.pools[0]=self.cost(dict(mana_cost=p['payment']))
        if mode=='ninjutsu_payment':self.pools[0]=self.cost(dict(mana_cost=p['ninjutsu']))
        if mode=='declined' and p['payment']:
            self.pools[0]=[0]*6
            for i,n in enumerate(['Forest','Forest','Mountain']):self.add(n,'payment_land_'+str(i))
        self.emit('turn',player=1 if mode=='owner_destination' else 0,value=0)
        self.emit('present_ai_dialogs',value=1);self.emit('discard_policy',value=1)
        for pl in (0,1):self.set_pool(pl,self.pools[pl])
        self.ready=len(self.steps)

    def eligible(self,ref,target):
        e=self.entities[ref]
        if e['zone']!='battlefield':return False
        if self.p['selector']!='land' and e['player']!=target:return False
        r=self.oracle[e['name']];types=r['type_line'].split(' — ')[0].split()
        if 'Shroud' in r.get('keywords',[]):return False
        selector=self.p['selector']
        return ('Land' in types if selector=='land' else 'Artifact' in types if selector=='artifact' else
                'Enchantment' in types if selector=='enchantment' else bool({'Artifact','Enchantment'}&set(types)) if selector=='artifact_or_enchantment' else
                'Creature' in types and (selector!='nonblack_creature' or 'B' not in r.get('colors',[])))

    def kill(self,ref,sacrifice=False):
        e=self.entities[ref]
        if not sacrifice and 'Indestructible' in self.oracle[e['name']].get('keywords',[]):return
        self.move(ref,'graveyard')
        if ref=='victim' and 'theft' in self.entities and self.entities['theft']['zone']=='battlefield':self.move('theft','graveyard')
        if ref=='source' and 'grant' in self.entities and self.entities['grant']['zone']=='battlefield':self.move('grant','graveyard')

    def removal_effect(self,amount,target=1,combat=True):
        p=self.p;mode=self.mode
        if amount<=0 or not combat and p['scope']=='combat':return
        choices=[r for r in ('victim','victim2','own_land','own_land2','later_victim','wall','idle','grant','source') if r in self.entities and self.eligible(r,target)]
        if p['payment']:
            # No payment-acceptance input is offered on the empty-target path.
            # This control observes no effect; the separately paid zero-target
            # control has eligible lands and deliberately chooses actual Done.
            if not choices:return
            if mode=='declined':self.emit('choose_cancel_mana',player=0);return
            if not all(a>=b for a,b in zip(self.pools[0],self.cost(dict(mana_cost=p['payment'])))):return
            self.spend(p['payment'])
            if mode=='mixed_lands':chosen=['victim','own_land']
            elif mode=='own_lands':chosen=['own_land','own_land2']
            else:chosen=choices[:0 if mode=='zero_targets' else 1 if mode=='one_target' else 2]
            for ref in chosen:self.emit('choose_target',player=0,ref=ref)
            if len(chosen)<2 and any(r not in chosen for r in choices):
                self.emit('choose_optional_target',player=0,ref=next(r for r in choices if r not in chosen))
            elif len(chosen)<2:
                # Numot still offers its second up-to selector after its only
                # remaining land was selected. End with actual Done, no target.
                self.emit('choose_cancel_target',player=0)
                if mode=='two_hits' and self.owner==0:
                    # Captured real selector re-offer after the last land:
                    # Numot overwrites packed damage history in targets[1]
                    # with first combat's second target controller. Stop the
                    # observed extra source0 menu without modeling an extra
                    # Oracle trigger, payment or destruction as expected.
                    self.emit('choose_cancel_target',player=0)
            for ref in chosen:self.kill(ref)
            return
        if not choices:
            # The AI mandatory outer trigger reaches Blind Zealot's optional
            # selector even when no target exists; its real Cancel preserves
            # both source and contrary permanents. Human outer decline does
            # not open this selector and must not receive an extra input.
            if p['effect']=='sacrifice_source_then_destroy' and self.owner==1:
                self.emit('choose_cancel_target',player=0)
            return
        if mode=='declined':
            # Human target cancellation is an actual offered optional choice.
            if not p['input_profile']['outer_optional']:self.emit('choose_optional_target',player=0,ref=choices[0])
            return
        if p['input_profile']['outer_optional'] and self.owner==0:self.emit('choose_trigger_source',player=0,ref='source')
        chosen=choices[0]
        self.emit('choose_target',player=0,ref=chosen)
        if p['effect']=='sacrifice_source_then_destroy':self.kill('source',True)
        self.kill(chosen,p['effect']=='targeted_sacrifice')

    def return_effect(self,amount,target=1,combat=True):self.removal_effect(amount,target,combat)

    def receipts(self):
        self.emit('assert_unused_choices',value=0)
        super().receipts()

    def combat(self):
        mode=self.mode;self.emit('begin_attackers');legal=mode!='fresh_attack'
        self.emit('attack',ref='source',value=int(legal))
        if legal:self.entities['source']['tapped']=True
        self.emit('finish_attackers');cut=len(self.steps);self.emit('begin_blockers')
        blocked=mode in ('blocked','keyword_block','artifact_block','color_block','second_block')
        attempted=blocked or mode=='contrary_block'
        if attempted:
            self.emit('block',ref='wall',other='source',value=int(blocked))
            self.emit('assert_card',ref='wall',property='blocking_count',value=int(blocked))
        if mode=='second_block':
            self.emit('block',ref='second_blocker',other='source',value=0)
            self.emit('assert_card',ref='second_blocker',property='blocking_count',value=0)
        self.emit('advance_combat')
        amount=self.power('source') if legal else 0
        if blocked and legal:
            back=self.power('wall');self.entities['wall']['damage']+=amount;self.entities['source']['damage']+=back
            if self.entities['wall']['damage']>=self.entities['wall']['toughness']:self.kill('wall')
            if self.entities['source']['damage']>=self.entities['source']['toughness']+self.entities['source']['boost']:self.kill('source')
            amount=0
        self.life[1]-=amount;self.removal_effect(amount)
        self.emit('combat_damage');self.emit('resolve');self.emit('end_combat');return cut

    def establish_foreign_owner(self):
        # Opponent really casts the control Aura on its legitimate own turn.
        self.emit('choose_target',player=1,ref='victim');self.emit('cast',ref='theft',value=1)
        self.emit('assert_stack',value=1)
        self.spend(self.oracle['Control Magic']['mana_cost'],1);self.move('theft','battlefield');self.emit('resolve')
        self.emit('assert_mana_total',player=1,value=self.mana[1])
        for color,value in enumerate(self.pools[1]):self.emit('assert_mana',player=1,color=color,value=value)
        self.entities['victim']['player']=1
        self.emit('bind_permanent_copy',player=1,value=self.rows[self.entities['victim']['name']]['csvid'],index=0,ref='victim')
        for pl in (0,1):self.set_pool(pl,[0]*6)
        self.emit('advance_turn');self.emit('untap_step',player=0);self.emit('turn',player=0,value=0)
        self.entities['source']['tapped']=False
        # Actual turn advancement empties floated pools; restore the fixture's
        # explicit resources before combat and keep both ledgers synchronized.
        for pl in (0,1):self.set_pool(pl,[10]*6)
        self.receipts()

    def run(self):
        mode=self.mode;cut=self.ready
        if mode=='owner_destination':self.establish_foreign_owner()
        if self.casting:
            if mode in ('insufficient_mana','wrong_color'):
                self.emit('cast',ref='source',value=0);self.emit('assert_card',ref='source',property='in_hand',value=1)
            else:self.spell('source',self.name,entity=True)
        if self.hand_mode:
            cut=self.ninjutsu()
        elif self.boundary:
            self.spell('grant','Hermetic Study','source',True);cut=len(self.steps)
            sick=mode=='noncombat_sick'
            if sick:self.emit('assert_card',ref='source',property='sick',value=1)
            self.emit('can_activate',ref='grant',value=int(not sick))
            if not sick:
                target=0 if mode=='noncombat_self' else 1
                self.emit('choose_player',player=0,value=target);self.emit('activate',ref='grant',value=1)
                self.emit('assert_stack',value=1);self.entities['source']['tapped']=True
                self.emit('assert_card',ref='source',property='tapped',value=1);self.emit('assert_card',ref='grant',property='tapped',value=0)
                self.life[target]-=1;self.removal_effect(1,target,False);self.emit('resolve')
        elif not self.casting or mode=='fresh_attack':
            if mode in ('pump','cleanup'):
                self.spell('helper','Giant Growth','source');self.entities['source']['boost']+=3
            cut=self.combat()
            if mode=='two_hits' and self.entities['source']['zone']=='battlefield':self.receipts();self.next_own_turn();self.combat()
            if mode=='source_bounce' and self.entities['source']['zone']=='battlefield':
                self.receipts();self.spell('helper','Unsummon','source');self.bounce('source')
            if mode in ('pump','cleanup'):
                self.receipts()
                for pl in (0,1):self.set_pool(pl,[0]*6)
                self.emit('cleanup');self.emit('refresh')
                for e in self.entities.values():e.update(boost=0,damage=0)
        self.receipts();steps=self.steps
        if mode in ('ready_snapshot','combat_snapshot','noncombat_snapshot','ninjutsu_pending_snapshot'):
            point=self.ready if mode=='ready_snapshot' else cut;replay=steps[point:]
            steps=copy.deepcopy(steps[:point])+[self.step('capture')]+copy.deepcopy(replay)+[self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+copy.deepcopy(replay)+[self.step('assert_recorded_state')]
        gaps=['Multiplayer, redirection, simultaneous sources, and target loss through an invented damage-trigger priority window are not claimed.',
              'No generic arbitrary triggered-target availability probe exists; sole contrary target/no-effect controls do not claim a direct UI permission observation.',
              'AI voluntary optional-trigger/payment decline is pending unless an actual UI choice is offered; shortage is tested independently and is never relabeled decline.',
              'Regeneration response interactions beyond indestructible destruction/sacrifice are a later real-menu control.']
        case=dict(name=f'CombatTargetedRemovalRecipe_{self.csvid}_{self.requested}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=steps,
                  _recipe=dict(family=FAMILY,version=2,variant=self.requested,params=copy.deepcopy(self.d['params']),oracle_text=self.d['oracle_text'],untested_components=gaps))
        if self.owner:_mirror(case)
        return case


def expand(name,csvid,d,p,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or any(v not in variants(p) for v in selected):raise ValueError('distinct supported removal variants required')
    return [RemovalBoard(name,csvid,d,p,v,rows,oracle).run() for v in selected]
