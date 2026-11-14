"""Whole-Oracle entry multi-tutors with real source casts and hidden-zone choices.

Immutable exchange actions supply actual Creature casting/reentry/snapshots;
library-search predicates and selection's owned-zone ledger supply independent
filter/conservation receipts. Sources never masquerade as spells or lose text.
"""
import copy
import json
import re
from pathlib import Path

import hand_exchange_recipes as exchange
from library_search_recipes import eligible as tutor_eligible, wave6_exact as exact_pin
from entry_trigger_value_selection_recipes import SelectionLedger, PALETTE

FAMILY='entry_tutor_trigger'
DATA=Path(__file__).parent/'data/entry_tutor_trigger_wave8_profiles.json'
WORDS={'two':2,'three':3}
VALUE_WITNESSES=['Champion of the Parish','Elvish Mystic','Signal Pest','Dryad Arbor','Tarfire',
                 'Darksteel Relic','Elvish Visionary','Artisan of Kozilek','Blasphemous Act',
                 'Griselbrand','Jin-Gitaxias, Core Augur']
ZONE_OPS={'assert_zone_card','assert_zone_contains','assert_zone_multiplicity','assert_zone_multiplicity_max','assert_hand_graveyard_total'}


def pins():return json.loads(DATA.read_text(encoding='utf8'))


def mechanism(record):
    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or 'Creature' not in record.get('type_line','').partition(' — ')[0].split()
            or any(not str(record.get(k,'')).isdigit() for k in ('power','toughness')) or int(record['toughness'])<=0):
        raise ValueError('entry tutor requires a normal fixed-stat Creature source')
    prefix=r'When (?:this creature|'+re.escape(record['name'])+r') enters, '
    clauses=[line for line in record['oracle_text'].splitlines() if re.match(prefix,line)]
    if len(clauses)!=1:raise ValueError('entry tutor requires one complete entry search clause')
    body=re.sub(prefix,'',clauses[0],count=1);p=None
    ending=r', reveal them, put them into your hand, then shuffle\.'
    match=re.fullmatch(r'you may search your library for up to (two|three) cards named (.+)'+ending,body)
    if match:
        p=dict(quantity=WORDS[match[1]],predicate=dict(scope='named',names=[match[2]],limit=None))
    match=re.fullmatch(r'you may search your library for up to (two|three) creature cards with mana value (\d+) or less'+ending,body)
    if match:
        p=dict(quantity=WORDS[match[1]],predicate=dict(scope='creature',names=[],limit=dict(field='mana_value',op='le',value=int(match[2]))))
    match=re.fullmatch(r'you may search your library for any number of cards that have mana value (\d+)'+ending,body)
    if match:
        p=dict(quantity='any',predicate=dict(scope='card',names=[],limit=dict(field='mana_value',op='eq',value=int(match[1]))))
    if p is None:raise ValueError('unsupported complete entry multi-tutor clause')
    return dict(p,entry_clause=clauses[0],gaps=[line for line in record['oracle_text'].splitlines() if line!=clauses[0]])


def spec(d,record):
    from recipes import cost_from_oracle
    if (d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2
            or not isinstance(d.get('variants'),list) or not d['variants'] or len(set(d['variants']))!=len(d['variants'])):
        raise ValueError('entry multi-tutor requires explicit version2 distinct variants')
    q=d.get('params')
    if not isinstance(q,dict) or set(q)!={'schema','mechanism','presentation'} or type(q['schema']) is not int or q['schema']!=1:
        raise ValueError('entry multi-tutor requires explicit mechanism and actual UI schema')
    parsed=mechanism(record);profile=pins()['sources'].get(record['name'])
    if (not profile or not exact_pin(profile['oracle'],record) or d.get('oracle_text')!=record['oracle_text']
            or not exact_pin(q['mechanism'],parsed) or not exact_pin(profile['mechanism'],parsed) or not exact_pin(q['presentation'],profile['presentation'])):
        raise ValueError('entry multi-tutor whole source/Oracle/presentation changed')
    presentation=q['presentation']
    if (not isinstance(presentation,dict) or set(presentation)!={'human_optional_trigger','sequential_single_search'}
            or any(type(v) is not bool for v in presentation.values())
            or presentation['sequential_single_search']!=(parsed['quantity']=='any')):
        raise ValueError('unreviewed entry multi-tutor input presentation')
    return dict(parsed,family=FAMILY,csvid=profile['csvid'],source_name=record['name'],scope=parsed['predicate']['scope'],
                presentation=copy.deepcopy(presentation),mana=cost_from_oracle(record),creature=True,land=False,
                legendary='Legendary' in record['type_line'],power=int(record['power']),toughness=int(record['toughness']))


def matches(record,p):return tutor_eligible(record,p['predicate'])


def variants(p):
    modes=['search','payment','insufficient_mana','wrong_color','countered','accepted_zero','one_selected','two_selected',
           'match1','match2','match3','match4','none_match','duplicates','last_choice','cleanup',
           'ready_snapshot','pending_snapshot','resolved_snapshot','bounce_recast','blink_source','nonmatching_entry','source_destroyed']
    if p['presentation']['human_optional_trigger']:modes.append('empty_library')
    result=[f'{mode}_{owner}' for owner in (0,1) for mode in modes]
    if p['presentation']['human_optional_trigger']:result.append('whole_decline_0')
    return result


def core(p):
    return dict(mode='entry',recipient='self',operations=[],cost=dict(mana='',tap=False,sacrifice=False,pay_life=0,discard=0),
                keywords=[],mana_ability=False,family='hand_exchange',activation_mana=[0]*6,mana=p['mana'],
                creature=True,land=False,legendary=p['legendary'],power=p['power'],toughness=p['toughness'],untested_rules=p['gaps'],instant=False)


def validate_witnesses(oracle,rows):
    data=pins()
    for name,record in data['witnesses'].items():
        if name not in rows or not exact_pin(oracle.get(name),record):raise ValueError('entry multi-tutor witness full pin changed: '+name)
    exchange.validate_witnesses(oracle,rows)
    for name in VALUE_WITNESSES:
        record=data['witnesses'][name]
        if record['layout']!='normal' or record.get('card_faces') or rows[name]['kind']!='card' or rows[name]['handler'] in (None,'NULL','card_dummy'):
            raise ValueError('multi-tutor boundary witness must be an implemented normal primary: '+name)
    for name,profile in data['sources'].items():
        if (oracle.get(name)!=profile['oracle'] or name not in rows or rows[name]['csvid']!=profile['csvid']
                or rows[name]['kind']!='card' or rows[name]['handler'] in (None,'NULL','card_dummy')):
            raise ValueError('entry multi-tutor source identity/implementation changed: '+name)


def library_fixture(name,p,mode,oracle):
    if p['scope']=='named':yes=[p['predicate']['names'][0]]*5;no=['Forest','Tarfire','Darksteel Relic','Elvish Visionary']
    elif p['scope']=='creature':
        yes=['Dryad Arbor','Champion of the Parish','Signal Pest','Elvish Mystic'];no=['Tarfire','Darksteel Relic','Elvish Visionary','Forest']
    else:
        yes=['Artisan of Kozilek','Blasphemous Act',name];no=['Griselbrand','Jin-Gitaxias, Core Augur','Forest','Tarfire']
    if not all(matches(oracle[n],p) for n in yes) or any(matches(oracle[n],p) for n in no):raise ValueError('entry tutor independent library predicate fixtures disagree')
    if mode=='empty_library':return []
    if mode=='none_match':return list(no)
    if mode.startswith('match') and mode[-1].isdigit():return [yes[i%len(yes)] for i in range(int(mode[-1]))]+no
    if mode=='duplicates':return [yes[0]]*4+no
    return yes+no


class TutorLedger(SelectionLedger):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs);self.shuffles=[0,0];self.inspected=None

    def state(self):return super().state(),copy.deepcopy(self.shuffles),copy.deepcopy(self.inspected)

    def restore(self,saved):
        base,shuffles,inspected=saved;super().restore(base);self.shuffles=copy.deepcopy(shuffles);self.inspected=copy.deepcopy(inspected)

    def search(self):
        from recipes import step
        self.inspected=None
        if self.mode=='whole_decline':return []
        p=self.p;owner=self.owner;remaining=self.zones['library'][owner];legal=[n for n in remaining if matches(self.oracle[n],p)]
        if self.mode=='last_choice':legal.reverse()
        cap=len(remaining) if p['quantity']=='any' else p['quantity']
        want=0 if self.mode=='accepted_zero' else min(cap,1 if self.mode=='one_selected' else 2 if self.mode=='two_selected' else len(legal))
        chosen=legal[:want];actions=[]
        for name in chosen:
            self.inspected=list(remaining);actions.append(step('choose_zone_card',player=owner,value=self.rows[name]['csvid']));remaining.remove(name)
        if remaining and (p['presentation']['sequential_single_search'] or len(chosen)<cap):
            self.inspected=list(remaining)
            fallback=next((n for n in remaining if matches(self.oracle[n],p)),remaining[0])
            actions.append(step('choose_optional_zone',player=owner,value=self.rows[fallback]['csvid']))
        self.zones['hand'][owner].extend(chosen);self.shuffles[owner]+=1
        return actions

    def receipts(self):
        from recipes import step
        out=[]
        for original in super().receipts():
            s=dict(original)
            if s['op']=='assert_shuffle_count':s['value']=self.shuffles[s['player']]
            if s['op']=='assert_zone_card' and s['zone']=='library' and self.shuffles[s['player']]:continue
            out.append(s)
        if self.inspected:
            out += [step('assert_offered_count',value=len(self.inspected)),step('assert_offered_selection_required',value=0)]
            out += [step('assert_offered_legal_card',index=self.rows[name]['csvid'],value=int(matches(self.oracle[name],self.p))) for name in sorted(set(self.inspected))]
        return out


def expand(name,csvid,d,p,selected,rows,oracle):
    from recipes import card,step,cost_from_oracle
    if csvid!=p['csvid'] or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(p)):
        raise ValueError('entry multi-tutor requires matching identity and distinct supported variants')
    validate_witnesses(oracle,rows);result=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1);owner=int(owner)
        underlying={'search':'effect','countered':'countered_cast','blink_source':'blink_source'}.get(mode,mode)
        if underlying not in ('effect','payment','insufficient_mana','wrong_color','countered_cast','cleanup','ready_snapshot','pending_snapshot','resolved_snapshot','bounce_recast','blink_source','source_destroyed'):underlying='effect'
        case=exchange.expand(name,csvid,dict(d,target_choices=0),core(p),[f'{underlying}_{owner}'],rows,oracle)[0]
        # A bounded hand keeps cleanup about entry/search persistence, not an
        # unrelated maximum-hand discard policy. Actual source types stay put.
        case['setup']=[item for item in case['setup'] if item['zone']!='library' and not (item['zone']=='hand' and item['ref']!='source' and item['name'] not in ('Plains','Giant Growth'))]
        library=library_fixture(name,p,mode,oracle)
        for player in (0,1):case['setup'] += [card(n,f'tutor_library_{player}_{i}',player,'library') for i,n in enumerate(library)]
        if mode=='none_match':
            positive=p['predicate']['names'][0] if p['scope']=='named' else 'Champion of the Parish' if p['scope']=='creature' else 'Blasphemous Act'
            for zone in ('hand','graveyard','exile'):case['setup'].append(card(positive,f'wrong_zone_{zone}',owner,zone))
            case['setup'].append(card(positive,'enemy_library_match',1-owner,'library'))
        if mode=='nonmatching_entry':
            case['setup'].append(card('Hill Giant','nonmatching',owner,'hand'))
            case['steps'] += [step('cast',ref='nonmatching',value=1),step('assert_stack',value=1),step('resolve'),step('assert_choices_consumed')]
            expected=[20-a-b for a,b in zip(p['mana'],cost_from_oracle(oracle['Hill Giant']))]
            case['steps'] += [step('assert_mana',player=owner,color=i,value=n) for i,n in enumerate(expected)]
        world=TutorLedger(case,p,mode,owner,rows,oracle);steps=[];saved=None
        for original in case['steps']:
            s=dict(original);op=s['op'];ref=s.get('ref')
            if op=='capture':saved=world.state()
            if op=='restore':world.restore(saved)
            if op=='cast' and ref in ('source','nonmatching'):
                if s['value']:world.move(ref,'stack');world.pending='source' if ref=='source' else 'other'
                elif mode=='countered':world.move('source','graveyard')
            if op=='activate' and ref in ('bouncer','blinker'):world.pending='bounce' if ref=='bouncer' else 'blink'
            if op=='destroy' and ref in world.entities:world.move(ref,'graveyard')
            if op=='resolve' and world.pending:
                pending=world.pending;world.pending=None
                if pending=='bounce':world.move('source','hand')
                elif pending=='other':world.move('nonmatching','battlefield')
                else:
                    if pending=='source':world.move('source','battlefield')
                    world.entities['source']['tapped']=pending=='blink'
                    if p['presentation']['human_optional_trigger'] and owner==0 and mode!='whole_decline':steps.append(step('choose_trigger',player=owner,ref='source'))
                    steps.extend(world.search());world.entered+=1
                steps.append(s);steps.extend(world.receipts());continue
            if op in ZONE_OPS:continue
            if op=='assert_zone':
                s['value']=world.count(s['player'],s['zone'])
                if s['zone']=='hand' and s['player']==owner and world.pending=='source':s['value']+=1
            steps.append(s)
            if op=='cast' and ref=='source' and s['value']:steps.append(step('assert_card',ref='source',property='in_hand',value=0))
        case['name']=f'EntryTutorTriggerRecipe_{csvid}_{variant}'
        case['steps']=[step('observe_search_filters',value=1)]+steps+world.receipts()
        gaps=p['gaps']+['multiplayer','control changes','replacement effects','simultaneous same-name optional live triggers','separate priority between entry and ETB resolution']
        if not p['presentation']['human_optional_trigger']:gaps += ['whole-action decline versus accepted empty-library search has no distinct offered UI; exact controls remain pending/noncoverage']
        case['_recipe']=dict(family=FAMILY,version=2,variant=variant,params=d['params'],oracle_text=d['oracle_text'],entry_clause=p['entry_clause'],untested_components=gaps)
        result.append(case)
    return result
