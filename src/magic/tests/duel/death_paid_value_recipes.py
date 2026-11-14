"""Whole fixed paid death value through actual deaths, mana and callbacks."""
import copy,json,re
from pathlib import Path
from death_counter_extended_recipes import World as ZoneWorld
from death_counter_trigger_recipes import exact

FAMILY='death_paid_value'
GAPS=['independent death-to-trigger announcement or response window',
      'player-target cancellation policy: permanent fallback observer is not a player selector',
      'AI discretionary decline and arbitrary independent trigger ordering',
      'paid acceptance after a token corpse has ceased to exist for return effects']


def pins():
    return json.loads((Path(__file__).parent/'data/death_paid_value_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record,dict) or record.get('layout')!='normal' or record.get('card_faces') or
        record.get('selected_face') or record.get('keywords') or '\n' in record.get('oracle_text','')):
        raise ValueError('whole normal single fixed paid-death clause required')
    types=set(record.get('type_line','').partition(' \u2014 ')[0].split())
    if not types&{'Creature','Artifact','Enchantment'} or 'Aura' in record.get('type_line',''):
        raise ValueError('standalone paid-death permanent required')
    if 'Creature' in types and any(not re.fullmatch(r'[1-9][0-9]*',record.get(k,'')) for k in('power','toughness')):
        raise ValueError('surviving numeric source creature required')
    text=record.get('oracle_text','')
    m=re.fullmatch(r'(.*?), (if it wasn\'t sacrificed, )?you may pay ((?:\{(?:[1-9]|[WUBRG])\})+)\. If you do, (.*?)\.',text)
    if not m:raise ValueError('complete fixed payment and conditional effect required')
    head,condition,payment,body=m.groups()
    predicate={'Whenever a creature dies':'creature',
               'Whenever an artifact you control is put into a graveyard from the battlefield':'artifact_controlled',
               'Whenever this creature or another Vampire you control dies':'vampire_controlled',
               'Whenever a creature is put into your graveyard from the battlefield':'creature_owned',
               'When this creature dies':'self'}.get(head)
    if not predicate or condition and predicate!='artifact_controlled':raise ValueError('unreviewed paid death predicate')
    target=False;gain=0;amount=1
    if n:=re.fullmatch(r'you gain ([1-3]) life',body):effect='gain';amount=int(n[1])
    elif body=='draw a card':effect='draw'
    elif n:=re.fullmatch(r'target player loses ([1-3]) life and you gain ([1-3]) life',body):
        effect='drain';amount=int(n[1]);gain=int(n[2]);target=True
    elif body=='target player discards a card':effect='discard';target=True
    elif body=='return that card to your hand' and predicate=='creature_owned':effect='return_hand'
    elif body=="return it to the battlefield tapped under its owner's control" and predicate=='self':effect='return_tapped'
    else:raise ValueError('complete reviewed fixed paid receipt required')
    if (head.startswith('When this creature') or predicate=='vampire_controlled') and 'Creature' not in types:
        raise ValueError('printed source creature identity required')
    return dict(predicate=predicate,not_sacrificed=bool(condition),payment=payment,effect=effect,amount=amount,gain=gain,target_player=target)


def spec(d,record):
    from recipes import cost_from_oracle
    expected=dict(schema=1,**oracle_model(record))
    if d.get('family')!=FAMILY or d.get('version')!=2 or d.get('oracle_text')!=record.get('oracle_text') or not exact(d.get('params'),expected):
        raise ValueError('typed complete paid-death declaration required')
    model=dict(copy.deepcopy(expected),family=FAMILY,mana=cost_from_oracle(record),
               trigger_mana=cost_from_oracle(dict(mana_cost=expected['payment'])),
               source_creature='Creature' in record['type_line'],power=int(record.get('power',0)),toughness=int(record.get('toughness',0)))
    selected=d.get('variants')
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported paid-death variants required')
    return model


def variants(m):
    modes=['cast_only','payment','insufficient_mana','countered','effect','trigger_unaffordable','source_foreign_owner',
           'token_death','opponent_death','stolen_death','lent_death','nonmatching','artifact_death','enchantment_death',
           'land_death','source_only','source_removed','source_bounced','source_exiled','victim_bounced','victim_exiled',
           'cleanup_once','lethal_death','zero_toughness','combat_death','sacrifice','bare_zones',
           'ready_snapshot','pending_snapshot','death_snapshot','resolved_snapshot']
    modes+=['wrong_color'] if any(m['mana'][1:]) else ['generic_source_colored']
    if m['predicate']!='self':modes+=['two_deaths','one_payment','repeat','source_simultaneous']
    if m['predicate']=='vampire_controlled':modes+=['changeling']
    if m['target_player']:modes+=['target_self','empty_target_hand'] if m['effect']=='discard' else ['target_self','damage_prevented']
    if m['effect']=='return_hand':modes+=['recast_returned']
    if m['effect']=='return_tapped':modes+=['second_death','new_object','return_untap']
    result=[f'{mode}_{p}' for p in(0,1) for mode in modes]+['decline_0']
    if not any(m['trigger_mana'][1:]):result+=['generic_trigger_colored_0','payment_cancel_0']
    return result


def validate_witnesses(oracle,registry):
    data=pins()
    for n,r in data['cards'].items():
        if not exact(r,oracle.get(n)) or not exact(data['registry'][n],registry.get(n)):raise ValueError('whole paid-death witness changed: '+n)
    for n,s in data['sources'].items():
        row=registry.get(n,{})
        if (not exact(s['oracle'],oracle.get(n)) or row.get('csvid')!=s['csvid'] or row.get('handler')!=s['handler'] or
            row.get('kind')!='card' or row.get('handler') in('NULL','0','card_dummy')):raise ValueError('implemented normal paid-death source changed: '+n)
        oracle_model(oracle[n]);p=data['presentation'].get(n)
        if not isinstance(p,dict) or set(p)!={'live_optional','dead_optional','corpse_selection'} or any(type(x) is not bool for x in p.values()):
            raise ValueError('actual source/legacy presentation required')


class World(ZoneWorld):
    def __init__(self,*args,mode,**kwargs):
        from recipes import card
        super().__init__(*args,**kwargs)
        self.mode=mode;self.presentation=pins()['presentation'][self.name];self.death_cut=None;self.return_number=0
        self.target=self.controller if mode=='target_self' else 1-self.controller
        if self.model['effect']=='discard':
            self.setup=[c for c in self.setup if c['zone']!='hand']
            self.zones['hand']=[[] if mode=='empty_target_hand' else ['Grizzly Bears'] for _ in(0,1)]
            self.setup += [card(n,f'hand_{p}_{i}',p,'hand') for p in(0,1) for i,n in enumerate(self.zones['hand'][p])]

    def stats(self,key='source'):
        from recipes import observe
        e=self.entities[key];checks=super().stats(key)
        if e['alive'] and 'sick' in e:checks += [observe(e['ref'],'sick',e['sick'])]
        return checks

    def effects(self):
        from recipes import step
        return [s for key,e in self.entities.items() if e['alive'] for s in self.stats(key)]+[
            step('assert_life',player=p,value=self.life[p]) for p in(0,1)]+[
            step('assert_unused_choices',value=0)]+self.mana_checks()

    def qualifies(self,key,sacrifice=False):
        e=self.entities[key];m=self.model;r=self.oracle[e['name']]
        if not e['alive'] or m['not_sacrificed'] and sacrifice:return False
        return {'self':key=='source','creature':'Creature' in r['type_line'],
                'artifact_controlled':'Artifact' in r['type_line'] and e['player']==self.controller,
                'vampire_controlled':e['player']==self.controller and (key=='source' or 'Vampire' in r['type_line'].partition(' \u2014 ')[2].split() or 'Changeling' in r.get('keywords',[])),
                'creature_owned':'Creature' in r['type_line'] and e['owner']==self.controller}[m['predicate']]

    def cast_source(self,pool):
        from recipes import step
        p=self.controller;run=[]
        if self.mode=='generic_source_colored' and p==0:run += [step('choose_mana',player=p,color=3) for _ in range(sum(pool))]
        self.zones['hand'][p].remove(self.name);self.source['alive']=True
        if self.model['source_creature']:self.source['sick']=1
        self.mana[p]=[0]*6
        return run+[step('cast',ref='source',value=1),step('assert_stack',value=1)]

    def trigger_pool(self,n=1):
        pool=[n*x for x in self.model['trigger_mana']]
        if self.mode=='trigger_unaffordable':return [0]*6
        if self.mode in('generic_trigger_colored','payment_cancel') or self.payment_decline():pool=[0,0,0,sum(pool),0,0]
        return pool

    def payment_decline(self):
        # The global creature/gain watcher has a looping outer-pass path.
        # Its real offered payment cancellation is already calibrated.
        return self.mode=='decline' and self.model['predicate']=='creature' and self.model['effect']=='gain'

    def plan(self,keys,sacrifice=False):
        from recipes import step
        if not self.source['alive']:return [],[]
        valid=[k for k in keys if self.qualifies(k,sacrifice)]
        if not valid:return [],[]
        p=self.controller;m=self.model;dying='source' in keys
        optional=self.presentation['dead_optional' if dying else 'live_optional']
        decline=self.mode=='decline' and not self.payment_decline() or self.mode=='token_death' and m['effect'] in('return_hand','return_tapped')
        if decline:
            # Token return controls explicitly decline payment; paid acceptance
            # after token disappearance remains a separately recorded gap.
            if p==0 and self.presentation['corpse_selection'] and not self.entities[valid[0]]['token']:
                return [step('choose_optional_zone',player=p,value=self.registry[self.entities[valid[0]]['name']]['csvid'])],[]
            return [],[]
        colored_generic=self.mode in('generic_trigger_colored','payment_cancel') or self.payment_decline()
        if (self.mana[p][3]<sum(m['trigger_mana']) if colored_generic else any(a<b for a,b in zip(self.mana[p],m['trigger_mana']))):
            if self.mode=='trigger_unaffordable' and p==0 and not dying and optional and m['predicate'] in('creature','artifact_controlled') and m['effect']=='gain':
                return [step('choose_trigger_source',ref=self.source['ref'],player=p)],[]
            return [],[]
        run=[];accepted=[]
        if p==0 and optional:run += [step('choose_trigger' if dying else 'choose_trigger_source',ref=self.source['ref'],player=p)]
        if self.mode=='payment_cancel' or self.payment_decline():return run+[step('choose_cancel_mana',player=p)],[]
        available=list(self.mana[p])
        for key in valid:
            cost=m['trigger_mana']
            colored_generic=self.mode=='generic_trigger_colored'
            if colored_generic:
                if available[3]<sum(cost):break
            elif any(a<b for a,b in zip(available,cost)):break
            e=self.entities[key]
            if self.presentation['corpse_selection'] and not e['token']:
                run += [step('choose_zone_card',player=p,value=self.registry[e['name']]['csvid'])]
            if colored_generic:
                if p==0:run += [step('choose_mana',player=p,color=3) for _ in range(sum(cost))]
                available[3]-=sum(cost)
            else:available=[a-b for a,b in zip(available,cost)]
            if m['target_player']:run += [step('choose_player',player=p,value=self.target)]
            if m['effect']=='discard' and self.zones['hand'][self.target]:
                chosen=self.zones['hand'][self.target][0]
                if self.target==0:run += [step('choose_zone_card',player=0,value=self.registry[chosen]['csvid'])]
            accepted.append(key)
        self.mana[p]=available
        return run,accepted

    def apply(self,accepted):
        from recipes import step
        m=self.model;p=self.controller;bind=[]
        for key in accepted:
            e=self.entities[key]
            if m['effect']=='gain':self.life[p]+=m['amount']
            elif m['effect']=='draw':
                if not self.zones['library'][p]:raise ValueError('terminal library loss is outside this fixed successful draw fixture')
                self.zones['hand'][p].append(self.zones['library'][p].pop(0))
            elif m['effect']=='drain':self.life[self.target]-=m['amount'];self.life[p]+=m['gain']
            elif m['effect']=='discard':
                if self.zones['hand'][self.target]:self.zones['graveyard'][self.target].append(self.zones['hand'][self.target].pop(0))
            elif not e['token']:
                self.zones['graveyard'][e['owner']].remove(e['name'])
                if m['effect']=='return_hand':
                    e['hand_copy']=self.zones['hand'][p].count(e['name']);self.zones['hand'][p].append(e['name'])
                else:
                    self.return_number+=1
                    e.update(ref=f'returned_{self.return_number}',player=e['owner'],alive=True,token=False,plus=0,minus=0,bp=0,bt=0,damage=0,tapped=1,sick=1,generation=e['generation']+1)
                    bind += [step('bind_permanent_copy',ref=e['ref'],player=e['owner'],value=self.registry[e['name']]['csvid'],index=0)]
        return bind

    def kill(self,key='victim',spell_key='death_spell',removal=None):
        from recipes import step
        r=self.oracle[self.entities[key]['name']]
        spell=removal or ('Murder' if 'Creature' in r['type_line'] else 'Stone Rain' if 'Land' in r['type_line'] else 'Naturalize')
        caster=self.controller if spell=='Stone Rain' else 1-self.controller
        run=self.fund(self.controller,self.trigger_pool())+self.spell(spell,spell_key,(key,),caster=caster)
        self.death_cut=len(self.actions)+len(run)
        plan,accepted=self.plan([key]);self.leave(key);bind=self.apply(accepted)
        receipt=[]
        if self.presentation['corpse_selection'] and not self.model['source_creature'] and key=='source':
            # No creature died. Decline an actual optional ghost menu safely;
            # a skipped optional intent is ignored by finish_choice_plan.
            plan=[step('observe_search_filters',value=1),step('choose_optional_zone',player=self.controller,value=self.registry['Grizzly Bears']['csvid'])]+plan
            receipt=[step('assert_unused_choices',value=0),step('assert_offered_count',value=0)]
        return run+plan+[step('resolve'),step('refresh')]+receipt+bind+self.effects()

    def sweep(self,include_source=False):
        from recipes import step
        # Wrath preserves noncreature watchers; Planar Cleansing exercises LKI
        # on a genuinely departing watcher, without a synthetic death event.
        spell='Planar Cleansing' if include_source else 'Wrath of God'
        keys=[k for k,e in self.entities.items() if e['alive'] and not e.get('indestructible') and ('Creature' in self.oracle[e['name']]['type_line'] or include_source and 'Land' not in self.oracle[e['name']]['type_line'])]
        eligible=sum(self.qualifies(k) for k in keys)
        pay=1 if self.mode=='one_payment' else max(eligible,1)
        # The sorcery caster must have the active main phase. All earlier
        # actions are finished; this phase handoff retains actual history.
        caster=1-self.controller
        run=[step('turn',player=caster,value=0)]+self.spell(spell,'sweep',caster=caster)
        run+=self.fund(self.controller,self.trigger_pool(pay));self.death_cut=len(self.actions)+len(run)
        plan,accepted=self.plan(keys)
        receipt=[]
        if include_source and self.presentation['corpse_selection']:
            # Preserve the wanted actual corpse. Observation mode only permits
            # decline when it is absent/illegal and the real menu allows cancel.
            run += [step('observe_search_filters',value=1)]
            receipt=[step('assert_unused_choices',value=0)]+[
                step('assert_offered_legal_card',index=self.registry[self.entities[k]['name']]['csvid'],value=1) for k in accepted]
        for key in keys:self.leave(key)
        bind=self.apply(accepted)
        return run+plan+[step('resolve'),step('refresh')]+receipt+bind+self.effects()

    def negative(self,key,exile=False):
        from recipes import step
        e=self.entities[key];creature='Creature' in self.oracle[e['name']]['type_line']
        spell=('Swords to Plowshares' if creature else 'Return to Dust') if exile else 'Boomerang'
        run=self.spell(spell,'negative_'+str(len(self.setup)),(key,))
        if exile and creature:self.life[e['player']]+=int(self.oracle[e['name']]['power'])+e['plus']-e['minus']+e['bp']
        self.leave(key,'exile' if exile else 'hand')
        return run+[step('resolve')]+self.effects()

    def age(self):
        for e in self.entities.values():
            if e['alive'] and 'sick' in e:e['sick']=0
        return super().age()

    def receipts(self):return super().receipts()+self.effects()


def expand(name,csvid,d,m,selected,registry,oracle):
    from recipes import step,cost_from_oracle
    validate_witnesses(oracle,registry);data=pins()
    if name not in data['sources'] or data['sources'][name]['csvid']!=csvid:raise ValueError('unreserved paid-death source')
    if not selected or len(selected)!=len(set(selected)) or set(selected)-set(variants(m)):raise ValueError('unsupported paid-death variants')
    cases=[]
    for variant in selected:
        mode,p=variant.rsplit('_',1);p=int(p)
        w=World(name,p,m,registry,oracle,mode=mode,bare=mode=='bare_zones');a=w.actions
        seeded=mode=='source_foreign_owner' or mode=='token_death' and m['predicate']=='self'
        w.add(name,'source',p,'battlefield' if seeded else 'hand',foreign=mode=='source_foreign_owner',token=mode=='token_death' and m['predicate']=='self')
        ready=pending=None
        if not seeded:
            pool=list(m['mana'])
            if mode=='generic_source_colored':pool=[0,0,0,sum(pool),0,0]
            if mode in('insufficient_mana','wrong_color'):
                if mode=='insufficient_mana':pool=[0]*6
                else:
                    color=next(c for c in range(1,6) if not pool[c]);pool=[pool[0],0,0,0,0,0];pool[color]=sum(m['mana'][1:])
                a+=w.fund(p,pool)+[step('cast',ref='source',value=0)]
            else:
                a+=w.fund(p,pool);ready=len(a);a+=w.cast_source(pool);pending=len(a)
                if mode=='countered':
                    w.add('Counterspell','counter',1-p,'hand');a+=w.fund(1-p,cost_from_oracle(oracle['Counterspell']))
                    w.mana[1-p]=[0]*6;w.zones['hand'][1-p].remove('Counterspell');w.zones['graveyard'][1-p].append('Counterspell')
                    w.leave('source');a += [step('announce_spell',ref='source'),step('cast_response',ref='counter',value=1),step('resolve_response')]+w.effects()
                else:a+=[step('resolve')]+w.effects()
        if mode not in('cast_only','payment','insufficient_mana','wrong_color','countered','generic_source_colored'):
            witness='Memnite' if m['predicate']=='artifact_controlled' else 'Child of Night' if m['predicate']=='vampire_controlled' else 'Grizzly Bears'
            who=1-p if mode in('opponent_death','lent_death') else p
            if mode=='nonmatching':witness='Grizzly Bears' if m['predicate'] in('artifact_controlled','vampire_controlled') else 'Sol Ring'
            if mode=='artifact_death':witness='Sol Ring'
            elif mode=='enchantment_death':witness='Exploration'
            elif mode=='land_death':witness='Forest'
            elif mode=='changeling':witness='Ghostly Changeling'
            target='source' if m['predicate']=='self' and mode not in('opponent_death','stolen_death','lent_death','nonmatching','artifact_death','enchantment_death','land_death') or mode=='source_only' or mode=='sacrifice' and m['predicate']=='vampire_controlled' else 'victim'
            if target=='victim':w.add(witness,'victim',who,foreign=mode in('stolen_death','lent_death'),token=mode=='token_death')
            if mode in('source_bounced','source_exiled','victim_bounced','victim_exiled'):
                key='source' if mode.startswith('source_') else target
                a+=w.negative(key,exile=mode.endswith('exiled'))
                if key=='source' and target!='source':a+=w.kill(target)
            elif mode=='source_removed':
                if target=='source':
                    a+=w.kill('source')
                    if m['effect']=='return_tapped':a+=w.negative('source',exile=True)
                else:a+=w.kill('source',spell_key='source_removal')+w.kill(target)
            elif mode in('two_deaths','one_payment','source_simultaneous'):
                if mode!='source_simultaneous':
                    w.add(witness,'second',p)
                    if m['source_creature']:
                        a+=w.spell('Indestructibility','watcher_shield',('source',),caster=p)+[step('resolve')]
                        w.entities['watcher_shield']['attached']='source';w.source['indestructible']=True
                a+=w.sweep(include_source=mode=='source_simultaneous')
            elif mode=='sacrifice':
                if 'Creature' not in oracle[w.entities[target]['name']]['type_line']:raise ValueError('altar fixture requires actual creature victim')
                w.add("Ashnod's Altar",'altar',p)
                a+=w.fund(p,w.trigger_pool())
                if p==0:a+=[step('choose_target',player=p,ref=w.entities[target]['ref'])]
                plan,accepted=w.plan([target],sacrifice=True);w.leave(target)
                # Altar actually addsCC before the paid death callback.
                w.mana[p][0]+=2;bind=w.apply(accepted)
                a+=plan+[step('activate',ref='altar',value=1),step('resolve'),step('refresh')]+bind+w.effects()
            elif mode=='combat_death':
                w.add('Craw Wurm','blocker',1-p);a+=w.age()+w.fund(p,w.trigger_pool())
                a += [step('begin_attackers'),step('attack',ref=w.entities[target]['ref'],value=1),step('finish_attackers'),step('begin_blockers'),
                      step('block',ref='blocker',other=w.entities[target]['ref'],value=1),step('advance_combat')]
                plan,accepted=w.plan([target]);damage=int(oracle[w.entities[target]['name']].get('power',0))
                if 'Lifelink' in oracle[w.entities[target]['name']].get('keywords',[]):w.life[w.entities[target]['player']]+=damage
                w.entities['blocker']['damage']=damage;w.leave(target);bind=w.apply(accepted)
                a+=plan+[step('combat_damage'),step('resolve'),step('refresh')]+bind+w.effects()+[step('end_combat')]
            else:
                if mode=='damage_prevented':
                    a+=w.spell('Safe Passage','prevention',caster=w.target)+[step('resolve')]
                    # Actual damage prevention cannot prevent the printed life loss.
                if mode=='new_object':
                    a+=w.spell('Burst of Strength','growth',('source',),caster=p);w.source['plus']+=1;a+=[step('resolve')]+w.effects()
                spell='Lightning Bolt' if mode=='lethal_death' else 'Disfigure' if mode=='zero_toughness' else None
                a+=w.kill(target,removal=spell)
                if mode=='repeat' and w.source['alive']:
                    w.add(witness,'repeat_victim',p);a+=w.kill('repeat_victim',spell_key='death_again')
                if mode=='second_death':a+=w.kill('source',spell_key='death_again')
                if mode=='cleanup_once':a+=[step('cleanup'),step('refresh')]+w.effects()
                if mode=='return_untap':
                    a+=w.age()+[step('begin_attackers'),step('attack',ref=w.source['ref'],value=1),step('finish_attackers'),step('begin_blockers'),step('advance_combat')]
                    w.source['tapped']=1;w.life[1-p]-=m['power'];a+=[step('combat_damage'),step('resolve')]+w.effects()+[step('end_combat')]
                if mode=='recast_returned':
                    e=w.entities[target];e.update(ref='recast',alive=True,sick=1)
                    a += [step('bind_hand_copy',ref='recast',player=p,value=registry[e['name']]['csvid'],index=e['hand_copy'])]+w.fund(p,cost_from_oracle(oracle[e['name']]))
                    w.zones['hand'][p].remove(e['name']);w.mana[p]=[0]*6;a += [step('cast',ref='recast',value=1),step('resolve')]+w.effects()
        checks=w.receipts();sequence=w.pre+a+checks
        if mode in('ready_snapshot','pending_snapshot','death_snapshot'):
            cut={'ready_snapshot':ready,'pending_snapshot':pending,'death_snapshot':w.death_cut}[mode]
            if cut is None:raise ValueError('real paid-death snapshot boundary missing')
            tail=a[cut:]+checks
            sequence=w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode=='resolved_snapshot':sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        cases.append(dict(name=f'DeathPaidValueRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=sequence,
                          _recipe=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],scope_gaps=GAPS)))
    return cases
