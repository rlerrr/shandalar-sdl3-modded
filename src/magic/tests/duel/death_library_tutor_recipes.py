"""Whole self-death searches, conditional corpse exile and actual Aura entry."""
import copy,json,re
from pathlib import Path
from death_counter_extended_recipes import World as ZoneWorld
from death_counter_trigger_recipes import exact
from library_search_recipes import eligible as library_eligible

FAMILY='death_library_tutor'


def pins():
    return json.loads((Path(__file__).parent/'data/death_library_tutor_wave8_profiles.json').read_text(encoding='utf8'))


def oracle_model(record):
    if (not isinstance(record,dict) or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
        not record.get('type_line','').startswith('Creature \u2014 ') or
        any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness')) or not int(record['toughness'])):
        raise ValueError('whole normal surviving numeric death-tutor Creature required')
    lines=record.get('oracle_text','').splitlines();keys=record.get('keywords',[])
    if keys:
        if keys!=['Flying'] or len(lines)!=2 or lines[0]!='Flying':raise ValueError('complete reviewed Flying line required')
        lines=lines[1:]
    if len(lines)!=1 or not lines[0].startswith('When this creature dies, '):raise ValueError('one complete self-dies search required')
    body=lines[0][len('When this creature dies, '):]
    if m:=re.fullmatch(r'you may search your library for (a basic land card|up to three Forest cards|an Elf card|a card named '+re.escape(record['name'])+r'), (reveal that card, )?put (?:it|that card|them) (into your hand|onto the battlefield(?: tapped)?), then shuffle\.',body):
        scope={'a basic land card':'basic_land','up to three Forest cards':'forest','an Elf card':'elf'}.get(m[1],'named')
        destination={'into your hand':'hand','onto the battlefield':'battlefield','onto the battlefield tapped':'battlefield_tapped'}[m[3]]
        return dict(scope=scope,maximum=3 if scope=='forest' else 1,participants='controller',destination=destination,
                    reveal=bool(m[2]),exile_source=False,keywords=list(keys))
    if body=='each player may search their library for up to two basic land cards, put them onto the battlefield, then shuffle.':
        return dict(scope='basic_land',maximum=2,participants='each',destination='battlefield',reveal=False,exile_source=False,keywords=[])
    if m:=re.fullmatch(r'you may exile it\. If you do, search your library for (an enchantment|a planeswalker) card, put (?:it|that card) onto the battlefield, then shuffle\.',body):
        return dict(scope='enchantment' if m[1]=='an enchantment' else 'planeswalker',maximum=1,participants='controller',
                    destination='battlefield',reveal=False,exile_source=True,keywords=list(keys))
    raise ValueError('complete typed self-dies tutor clause required')


def spec(declaration,record):
    from recipes import cost_from_oracle
    params=dict(schema=1,**oracle_model(record))
    if declaration.get('family')!=FAMILY or declaration.get('version')!=2 or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params):
        raise ValueError('typed complete death-tutor declaration required')
    model=dict(copy.deepcopy(params),family=FAMILY,mana=cost_from_oracle(record),power=int(record['power']),toughness=int(record['toughness']))
    selected=declaration.get('variants')
    if not isinstance(selected,list) or not selected or len(selected)!=len(set(selected)) or set(selected)-set(variants(model)):
        raise ValueError('explicit distinct supported death-tutor variants required')
    return model


def variants(model):
    modes=['cast_only','payment','insufficient_mana','wrong_color','countered','search','accepted_zero','no_match','empty_library',
           'single_match','duplicates','source_foreign_owner','source_token','source_bounced','source_exiled','source_blinked',
           'other_dies','lethal_death','zero_toughness','combat_death','sacrifice','bare_zones','cleanup_once',
           'post_search_draw','ready_snapshot','pending_snapshot','death_snapshot','resolved_snapshot']
    if model['maximum']>1:modes+=['one_selected']
    if model['scope'] not in ('named','enchantment'):modes+=['last_choice']
    if model['participants']=='each':modes+=['decline_controller','decline_opponent','decline_both']
    if model['destination']=='hand':modes+=['recast_found']
    if model['scope'] in ('basic_land','forest'):modes+=['mana_fetched','nonbasic_forest','snow_basic']
    if model['scope']=='elf':modes+=['kindred_filter','changeling_filter']
    if model['scope']=='enchantment':modes+=['aura_entry','aura_enemy','aura_shroud','aura_not_target','aura_departure']
    if 'Flying' in model['keywords']:modes+=['flying_combat','reach_combat']
    result=[f'{mode}_{p}' for p in (0,1) for mode in modes]
    if model['participants']=='controller':result+=['decline_0']
    return result


def eligible(record,model,name):
    return library_eligible(record,dict(scope=model['scope'],names=[name],limit=None))


def validate_witnesses(oracle,registry):
    data=pins()
    for name,record in data['cards'].items():
        if not exact(record,oracle.get(name)) or not exact(data['registry'][name],registry.get(name)):raise ValueError('whole death-tutor witness changed: '+name)
    for name,source in data['sources'].items():
        row=registry.get(name,{})
        if not exact(source['oracle'],oracle.get(name)) or row.get('csvid')!=source['csvid'] or row.get('handler')!=source['handler'] or row.get('kind')!='card' or row.get('handler') in ('NULL','0','card_dummy'):
            raise ValueError('implemented whole death-tutor source changed: '+name)
        model=oracle_model(oracle[name]);presentation=data['presentation'].get(name)
        if presentation!=dict(human_death_trigger=model['participants']=='controller',each_player_dialog=model['participants']=='each'):
            raise ValueError('actual death-source presentation required')


class World(ZoneWorld):
    def __init__(self,*args,deck=None,**kwargs):
        from recipes import card,step
        super().__init__(*args,**kwargs)
        self.setup=[c for c in self.setup if c['zone']!='library']
        self.zones['library']=[list(deck),list(deck)]
        for p in (0,1):self.setup += [card(n,f'library_{p}_{i}',p,'library') for i,n in enumerate(deck)]
        self.pre += [step('observe_search_filters',value=1)]
        self.inspected=None;self.fetched=[];self.death_cut=None

    def stats(self,key='source'):
        from recipes import step
        e=self.entities[key];checks=super().stats(key)
        if e['alive'] and 'Planeswalker' in self.oracle[e['name']]['type_line']:
            checks += [step('assert_counter',ref=e['ref'],counter_kind='loyalty',value=int(self.oracle[e['name']]['loyalty']))]
        return checks

    def effects(self):
        from recipes import step
        return [s for key,e in self.entities.items() if e['alive'] for s in self.stats(key)]+[step('assert_unused_choices',value=0)]+self.mana_checks()

    def search_plan(self,mode):
        from recipes import step
        m=self.model;p=self.controller;run=[];bind=[]
        whole_decline=mode=='decline'
        can_exile=not self.source['token']
        if m['participants']=='controller' and (whole_decline or m['exile_source'] and not can_exile):return run,bind
        if p==0 and m['participants']=='controller':run += [step('choose_trigger',ref=self.source['ref'],player=p)]
        if m['exile_source']:
            owner=self.source['owner'];self.zones['graveyard'][owner].remove(self.name);self.zones['exile'][owner].append(self.name)
        for who in ((0,1) if m['participants']=='each' else (p,)):
            decline=mode=='decline_both' or mode=='decline_controller' and who==p or mode=='decline_opponent' and who!=p
            if m['participants']=='each':run += [step('choose_dialog',player=who,value=int(decline))]
            if decline:continue
            remaining=self.zones['library'][who];legal=[n for n in remaining if eligible(self.oracle[n],m,self.name)]
            if mode=='last_choice':legal.reverse()
            if mode=='nonbasic_forest' and 'Taiga' in legal:legal.remove('Taiga');legal.insert(0,'Taiga')
            if mode=='snow_basic' and 'Snow-Covered Forest' in legal:legal.remove('Snow-Covered Forest');legal.insert(0,'Snow-Covered Forest')
            if mode.startswith('aura_'):legal=['Holy Strength']
            elif m['scope']=='enchantment':legal=[n for n in legal if 'Aura' not in self.oracle[n]['type_line']]
            desired=0 if mode=='accepted_zero' else 1 if mode in ('one_selected','post_search_draw') else m['maximum']
            chosen=legal[:desired]
            for n in chosen:
                self.inspected=list(remaining)
                run += [step('choose_zone_card',player=who,value=self.registry[n]['csvid'])]
                remaining.remove(n)
            if len(chosen)<m['maximum'] and remaining:
                self.inspected=list(remaining)
                run += [step('choose_optional_zone',player=who,value=self.registry[(next((n for n in remaining if eligible(self.oracle[n],m,self.name)),None) or remaining[0])]['csvid'])]
            self.shuffles[who]+=1
            for n in chosen:
                if m['destination']=='hand':self.zones['hand'][who].append(n)
                else:
                    key='found_'+str(len(self.fetched));index=sum(e['alive'] and e['player']==who and e['name']==n for e in self.entities.values())
                    # New entities describe real cards moved by the card's tutor;
                    # they never add setup objects or inject battlefield state.
                    e=dict(key=key,ref=key,name=n,player=who,owner=who,alive=True,token=False,plus=0,minus=0,bp=0,bt=0,damage=0,
                           tapped=int(m['destination']=='battlefield_tapped'),generation=0,flying=False,grounded=False)
                    self.entities[key]=e;self.fetched.append(key)
                    bind += [step('bind_permanent_copy',ref=key,player=who,value=self.registry[n]['csvid'],index=index)]
                    if 'Aura' in self.oracle[n]['type_line']:
                        run += [step('choose_target',ref='receiver',player=who)]
                        e['attached']='receiver';self.entities['receiver']['bp']+=1;self.entities['receiver']['bt']+=2
        return run,bind

    def death(self,mode,spell='Murder'):
        from recipes import step
        run=self.spell(spell,'death_spell',('source',))
        self.death_cut=len(self.actions)+len(run)
        self.leave('source');plan,bind=self.search_plan(mode)
        return run+plan+[step('resolve'),step('refresh')]+bind+self.effects()

    def search_checks(self):
        from recipes import step
        if self.inspected is None:return []
        return [step('assert_offered_count',value=len(self.inspected)),step('assert_offered_selection_required',value=0)]+[
            step('assert_offered_legal_card',index=self.registry[n]['csvid'],value=int(eligible(self.oracle[n],self.model,self.name))) for n in sorted(set(self.inspected))]

    def receipts(self):
        return super().receipts()+self.effects()+self.search_checks()


def deck_for(name,model,mode,oracle):
    scope=model['scope']
    deck=({'basic_land':['Grizzly Bears','Forest','Island','Taiga','Plains','Forest','Snow-Covered Forest','Lightning Bolt','Darksteel Citadel'],
           'forest':['Grizzly Bears','Forest','Taiga','Snow-Covered Forest','Forest','Island','Lightning Bolt'],
           'elf':['Grizzly Bears','Elvish Mystic','Sol Ring',name,'Forest','Lightning Bolt'],
           'named':[name,'Welkin Tern','Grizzly Bears',name,'Forest','Lightning Bolt'],
           'enchantment':['Grizzly Bears','Exploration','Holy Strength','Sol Ring','Fertile Ground','Forest'],
           'planeswalker':['Grizzly Bears','Chandra, Pyrogenius','Forest']}[scope])
    # Leave valid Kindred/Changeling witnesses in the observed mixed deck;
    # select a conventional Elf, never force a card rejected by a buggy filter.
    good=next(n for n in deck if eligible(oracle[n],model,name))
    if mode=='no_match':return ['Grizzly Bears','Lightning Bolt'] if scope!='named' else ['Welkin Tern','Lightning Bolt']
    if mode=='empty_library':return []
    if mode=='single_match':return [good,'Lightning Bolt']
    if mode=='duplicates':return [good]*4+['Lightning Bolt']
    if mode=='post_search_draw':return [good]+['Lightning Bolt']*4
    if mode=='kindred_filter':deck=deck+['Elvish Promenade']
    if mode=='changeling_filter':deck=deck+['Ghostly Changeling']
    return list(deck)


def expand(name,csvid,declaration,model,selected,registry,oracle):
    from recipes import step,cost_from_oracle
    validate_witnesses(oracle,registry);data=pins()
    if name not in data['sources'] or data['sources'][name]['csvid']!=csvid:raise ValueError('unreserved death-tutor source')
    if not selected or len(selected)!=len(set(selected)) or set(selected)-set(variants(model)):raise ValueError('unsupported death-tutor selection')
    cases=[]
    for variant in selected:
        mode,p=variant.rsplit('_',1);p=int(p)
        w=World(name,p,model,registry,oracle,bare=mode=='bare_zones',deck=deck_for(name,model,mode,oracle))
        seeded=mode in ('source_foreign_owner','source_token')
        w.add(name,'source',p,'battlefield' if seeded else 'hand',foreign=mode=='source_foreign_owner',token=mode=='source_token')
        if mode.startswith('aura_'):
            receiver='Deadly Insect' if mode=='aura_shroud' else 'Task Force' if mode=='aura_not_target' else 'Grizzly Bears'
            w.add(receiver,'receiver',1-p if mode=='aura_enemy' else p)
        ready=pending=None;a=w.actions
        if not seeded:
            pool=list(model['mana'])
            if mode=='insufficient_mana':pool=[0]*6
            elif mode=='wrong_color':
                other=next(c for c in range(1,6) if not pool[c]);pool=[pool[0],0,0,0,0,0];pool[other]=sum(model['mana'][1:])
            a+=w.fund(p,pool);ready=len(a)
            if mode in ('insufficient_mana','wrong_color'):a+=[step('cast',ref='source',value=0)]
            else:
                a+=[step('cast',ref='source',value=1),step('assert_stack',value=1)]
                w.source['alive']=True;w.zones['hand'][p].remove(name);w.mana[p]=[0]*6;pending=len(a)
                if mode=='countered':
                    w.add('Counterspell','counter',1-p,'hand');a+=w.fund(1-p,cost_from_oracle(oracle['Counterspell']))
                    w.mana[1-p]=[0]*6;w.zones['hand'][1-p].remove('Counterspell');w.zones['graveyard'][1-p].append('Counterspell')
                    w.leave('source');a += [step('announce_spell',ref='source'),step('cast_response',ref='counter',value=1),step('resolve_response')]+w.effects()
                else:a+=[step('resolve')]+w.effects()
        if mode not in ('cast_only','payment','insufficient_mana','wrong_color','countered'):
            if mode in ('source_bounced','source_exiled'):
                a+=w.spell('Boomerang' if mode=='source_bounced' else 'Swords to Plowshares','negative',('source',))
                if mode=='source_exiled':w.life[p]+=model['power']
                w.leave('source','hand' if mode=='source_bounced' else 'exile');a+=[step('resolve')]+w.effects()
            elif mode=='other_dies':
                w.add('Grizzly Bears','other',p);a+=w.spell('Murder','other_death',('other',));w.leave('other');a+=[step('resolve')]+w.effects()
            else:
                if mode=='source_blinked':a+=w.blink('source')
                if mode in ('combat_death','reach_combat','flying_combat'):
                    blocker='Giant Spider' if mode=='reach_combat' or mode=='combat_death' and 'Flying' in model['keywords'] else 'Grizzly Bears' if mode=='flying_combat' else 'Craw Wurm'
                    w.add(blocker,'blocker',1-p);a+=w.age()
                    blocked=mode!='flying_combat'
                    a += [step('begin_attackers'),step('attack',ref=w.source['ref'],value=1),step('finish_attackers'),step('begin_blockers'),
                          step('block',ref='blocker',other=w.source['ref'],value=int(blocked)),step('advance_combat')]
                    if blocked:
                        w.leave('source');plan,bind=w.search_plan(mode)
                        if model['power']>=int(oracle[blocker]['toughness']):w.leave('blocker')
                        else:w.entities['blocker']['damage']=model['power']
                        a+=plan+[step('combat_damage'),step('resolve'),step('refresh')]+bind+w.effects()+[step('end_combat')]
                    else:
                        w.source['tapped']=1;w.life[1-p]-=model['power']
                        a+=[step('combat_damage'),step('resolve')]+w.effects()+[step('end_combat'),step('turn',player=p,value=0)]
                        a+=w.death(mode)
                elif mode=='sacrifice':
                    w.add("Ashnod's Altar",'altar',p)
                    if p==0:a+=[step('choose_target',ref=w.source['ref'],player=p)]
                    w.leave('source');plan,bind=w.search_plan(mode)
                    w.mana[p][0]+=2
                    a+=plan+[step('activate',ref='altar',value=1),step('resolve'),step('refresh')]+bind+w.effects()
                else:
                    removal=('Lightning Bolt' if model['toughness']<=3 else 'Lightning Blast') if mode=='lethal_death' else ('Disfigure' if model['toughness']<=2 else 'Grasp of Darkness') if mode=='zero_toughness' else 'Murder'
                    a+=w.death(mode,spell=removal)
                if mode=='cleanup_once':a+=[step('cleanup'),step('refresh')]+w.effects()
                if mode=='post_search_draw':
                    a+=w.spell('Reach Through Mists','draw',caster=p)+[step('resolve')]
                    w.zones['hand'][p].append(w.zones['library'][p].pop(0));a+=w.effects()
                if mode=='recast_found':
                    found=next(n for n in w.zones['hand'][p] if eligible(oracle[n],model,name))
                    w.entities['recast']=dict(w.source,key='recast',ref='recast',name=found,owner=p,player=p,alive=True,plus=0,minus=0,bp=0,bt=0,damage=0,tapped=0)
                    a+=[step('bind_hand_copy',ref='recast',player=p,value=registry[found]['csvid'],index=0)]+w.fund(p,cost_from_oracle(oracle[found]))
                    w.zones['hand'][p].remove(found);w.mana[p]=[0]*6;a+=[step('cast',ref='recast',value=1),step('resolve')]+w.effects()
                if mode=='mana_fetched':
                    a+=w.age();key=next(k for k in w.fetched if w.entities[k]['player']==p);w.entities[key]['tapped']=1;w.mana[p][3]+=1
                    a+=[step('activate',ref=key,value=1)]+w.effects()
                if mode=='aura_departure':
                    a+=w.spell('Murder','receiver_death',('receiver',));w.leave('receiver');a+=[step('resolve')]+w.effects()
        checks=w.receipts();sequence=w.pre+a+checks
        if mode in ('ready_snapshot','pending_snapshot','death_snapshot'):
            cut={'ready_snapshot':ready,'pending_snapshot':pending,'death_snapshot':w.death_cut}[mode]
            if cut is None:raise ValueError('actual snapshot boundary missing')
            tail=a[cut:]+checks
            sequence=w.pre+a[:cut]+[step('capture')]+copy.deepcopy(tail)+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(tail)+[step('assert_recorded_state')]
        elif mode=='resolved_snapshot':sequence += [step('capture'),step('record_state'),step('ai_roundtrip'),step('restore'),step('assert_recorded_state')]
        cases.append(dict(name=f'DeathLibraryTutorRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,setup=w.setup,steps=sequence,
            _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                         scope_gaps=['independent response between death and trigger resolution','multiplayer','Aura entry with no eligible recipient','arbitrary optional legacy ordering'])))
    return cases
