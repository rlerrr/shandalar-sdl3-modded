"""Whole-Oracle Cascade calibration using real production nested free casting.

The accepted sixteen-case calibration remains byte-for-byte stable.  Complements
use its real input path and compare random bottoms by contents, never permutation.
"""
import copy,json,re
from pathlib import Path
from grant_recipes import LIBRARY
from cast_untap_trigger_recipes import pin,exact
from cast_temporary_stat_trigger_recipes import StatLedger

FAMILY='cascade_spell_effect'
DATA=Path(__file__).parent/'data/cascade_spell_effect_wave8_profiles.json'
CASCADE='Cascade (When you cast this spell, exile cards from the top of your library until you exile a nonland card that costs less. You may cast it without paying its mana cost. Put the exiled cards on the bottom in a random order.)'
EXALTED='Exalted (Whenever a creature you control attacks alone, that creature gets +1/+1 until end of turn.)'
HASTE='Haste (This creature can attack and {T} as soon as it comes under your control.)'

def pins():return json.loads(DATA.read_text(encoding='utf8'))
def classify(r):
    from recipes import cost_from_oracle
    if not r or r.get('layout')!='normal' or r.get('card_faces') or r.get('selected_face') or r.get('type_line','').startswith('Token '):raise ValueError('whole normal primary required')
    text=r.get('oracle_text','').splitlines()
    if text.count(CASCADE)!=1 or len(text)!=len(set(text)):raise ValueError('complete single Cascade clause required')
    rest=[s for s in text if s!=CASCADE];creature='Creature' in r['type_line'];effect='vanilla';amount=0;target=None;keys=['Cascade']
    if rest==[HASTE] and creature:effect='haste';keys+=['Haste']
    elif rest==[EXALTED] and r['type_line']=='Enchantment':effect='exalted';keys+=['Exalted']
    elif len(rest)==1:
        s=rest[0]
        if re.fullmatch(re.escape(r['name'])+r' deals 4 damage to target creature\.',s) and r['type_line']=='Instant':effect='damage';amount=4;target='creature'
        elif s=='You gain 4 life.' and r['type_line']=='Sorcery':effect='life';amount=4
        elif s=="Target creature can't block this turn." and r['type_line']=='Sorcery':effect='cannot_block';target='creature'
        elif s=="Return target permanent to its owner's hand." and r['type_line']=='Sorcery':effect='bounce';target='permanent'
        elif s=='Creatures you control get +1/+0 until end of turn.' and r['type_line']=='Instant':effect='team_power';amount=1
        else:raise ValueError('unmodeled whole primary clause')
    elif rest or not creature:raise ValueError('unmodeled complete primary')
    if set(r.get('keywords',[]))!=set(keys):raise ValueError('whole keyword identity changed')
    if creature and any(not re.fullmatch(r'\d+',r.get(k,'')) for k in ('power','toughness')):raise ValueError('numeric creature body required')
    cost=cost_from_oracle(r)
    if type(r.get('cmc')) not in (int,float) or r['cmc']!=sum(cost) or sum(cost)<=2:raise ValueError('fixed positive cost above calibrated free spell required')
    return dict(effect=effect,amount=amount,target=target,mana=cost,mv=sum(cost),creature=creature,power=int(r['power']) if creature else 0,toughness=int(r['toughness']) if creature else 0)
def parameters(p):return copy.deepcopy(p)
BOUNDARY={3:'Grizzly Bears',4:'Scathe Zombies',5:'Hill Giant',6:'Spined Wurm'}
def variants(p):
    modes=['accept','decline','scan_land','scan_equal','scan_higher','scan_mixed','decline_mixed','lower_boundary','zero_cost','no_eligible','empty_library','one_card','last_eligible','countered_parent','source_insufficient_mana','source_wrong_color']
    if p['target']:modes+=['own_target','no_legal_target','target_removed']
    if p['effect']=='bounce':modes+=['artifact_target','land_target','enchantment_target','planeswalker_target']
    if p['effect']=='exalted':modes+=['two_attackers','opponent_attack','source_removed']
    if p['effect']=='team_power':modes+=['late_creature']
    return [f'{m}_{w}' for w in (0,1) for m in modes]
def spec(d,r):
    if d.get('family')!=FAMILY or type(d.get('version')) is not int or d['version']!=2 or d.get('oracle_text')!=r.get('oracle_text'):raise ValueError('whole Oracle and exact version required')
    if not exact(pins()['sources'].get(r['name']),pin(r)):raise ValueError('source Oracle pin changed')
    p=classify(r)
    if not exact(d.get('params'),parameters(p)):raise ValueError('typed whole-clause parameters changed')
    vs=d.get('variants')
    if not isinstance(vs,list) or not vs or len(vs)!=len(set(vs)) or set(vs)-set(variants(p)):raise ValueError('unsupported calibration variants')
    return dict(p,family=FAMILY)
def validate_witnesses(o,rows):
    from creature_recipes import oracle_keywords
    data=pins()
    for n,p in dict(data['sources'],**data['witnesses']).items():
        if n not in rows or rows[n]['handler'] in ('0','NULL') or not exact(pin(o.get(n,{})),p):raise ValueError('whole source/witness pin changed: '+n)
        if n in data['sources']:
            if rows[n]['kind']!='card' or rows[n]['handler']=='card_dummy':raise ValueError('implemented primary required')
            classify(o[n])
        elif rows[n]['handler']=='card_dummy':oracle_keywords(o[n])
    for n,h in data['handlers'].items():
        if rows[n]['handler']!=h:raise ValueError('presentation handler changed: '+n)
    free=o['Grizzly Bears']
    if free['cmc']!=2 or free['mana_cost']!='{1}{G}' or free['oracle_text'] or free['power']!='2' or free['toughness']!='2':raise ValueError('independent positive-cost targetless free witness changed')

class CascadeLedger(StatLedger):
    def __init__(self,name,csvid,d,p,v,rows,o):
        who=int(v.rsplit('_',1)[1]);bridge=dict(scope='self',keywords=[],lifelink=False,death_draw=0,activation=None)
        super().__init__(name,csvid,d,bridge,'source_cast_'+str(who),rows,o)
        self.p=p;self.mode=v.rsplit('_',1)[0];self.variant=v;self.random_bottom=False;self.ordered_suffix=0
        self.mana[who]=list(p['mana'])
        from recipes import card,step
        self.free_name=BOUNDARY[p['mv']] if self.mode=='lower_boundary' else 'Memnite' if self.mode=='zero_cost' else 'Grizzly Bears'
        self.prefix=['Forest','Island'] if self.mode=='scan_land' else [name] if self.mode=='scan_equal' else ['Enormous Baloth'] if self.mode=='scan_higher' else ['Forest',name,'Enormous Baloth','Island'] if self.mode in ('scan_mixed','decline_mixed','last_eligible') else []
        self.suffix=list(LIBRARY)*3
        if self.mode in ('one_card','last_eligible'):self.suffix=[]
        self.eligible=self.mode not in ('no_eligible','empty_library');self.accept=self.eligible and self.mode not in ('decline','decline_mixed')
        library=self.prefix+[self.free_name]+self.suffix if self.eligible else ['Enormous Baloth']*5 if self.mode=='no_eligible' else []
        self.setup=[c for c in self.setup if not(c['player']==who and c['zone']=='library')]
        self.z['library'][who]=library
        self.setup += [card(n,f'cascade_library_{i}',who,'library') for i,n in enumerate(self.z['library'][who])]
        self.target_ref='idle' if self.mode=='own_target' else 'noncreature' if self.mode=='artifact_target' else 'target' if self.mode in ('land_target','enchantment_target','planeswalker_target') else 'enemy'
        if self.mode in ('land_target','enchantment_target','planeswalker_target'):
            self.add({'land_target':'Forest','enchantment_target':'Fervor','planeswalker_target':'Jace Beleren'}[self.mode],'target',1-who)
        if self.mode=='no_legal_target':
            self.setup=[c for c in self.setup if c['zone']!='battlefield'];self.e={k:e for k,e in self.e.items() if not e['alive']}
        if self.mode=='countered_parent':self.add('Counterspell','response',1-who,'hand')
        if self.mode=='late_creature':self.add('Grizzly Bears','late',who,'hand')
        if self.mode=='two_attackers':self.add('Grizzly Bears','ready_extra',who)
        if self.mode=='source_insufficient_mana':self.mana[who][0]-=1
        if self.mode=='source_wrong_color':self.mana[who]=[sum(p['mana']),0,0,0,0,0]
        self.pre=[step('reset_death_history'),step('turn',player=who,value=0)]+[step('set_mana',player=w,color=c,value=n) for w in (0,1) for c,n in enumerate(self.mana[w])]
        self.universe|=set(library)|{e['name'] for e in self.e.values()}
    def notify(self,ref):pass
    def kill(self,ref,explicit=False):
        if self.e[ref]['name']!=self.name:return super().kill(ref,explicit)
        if explicit:self.step('destroy',ref=ref)
        e=self.e[ref]
        if not e['alive']:raise ValueError('cannot kill absent source')
        e['alive']=False;self.z['graveyard'][e['player']].append(e['name'])
    def checks(self,stack=0):
        out=super().checks(stack)
        # Ordered suffix remains first; random bottom exact contents are checked
        # by zone size and complete multiplicities, with no guessed permutation.
        if self.random_bottom:out=[s for s in out if not(s['op']=='assert_zone_card' and s.get('zone')=='library' and s.get('player')==self.owner and s['index']>=self.ordered_suffix)]
        return out
    def real_cast(self):
        if self.mode in ('source_insufficient_mana','source_wrong_color','no_legal_target'):
            self.step('cast',ref='source',value=0);self.step('assert_unused_choices',value=0);self.actions+=self.checks();return
        if self.p['target']:self.step('choose_target',player=self.owner,ref=self.target_ref)
        # do_dialog's scripted branch offers this choice to either controller.
        if self.eligible:self.step('choose_dialog',player=self.owner,value=0 if self.accept else 1)
        self.step('cast',ref='source',value=1)
        self.spend(self.owner,self.p['mana']);self.z['hand'][self.owner].remove(self.name);self.e['source']['pending']=True
        # The nested free spell resolves in production real_play_spell_for_free.
        # Its real positive cost cannot be paid from the now-empty source pool.
        bottom=self.prefix+([] if self.accept else [self.free_name]) if self.eligible else list(self.z['library'][self.owner])
        self.z['library'][self.owner]=(self.suffix if self.eligible else [])+bottom
        self.random_bottom=len(bottom)>1;self.ordered_suffix=len(self.suffix) if self.eligible else 0
        if self.accept:
            r=self.o[self.free_name];e=dict(name=self.free_name,player=self.owner,alive=True,pending=False,creature=True,power=int(r['power']),toughness=int(r['toughness']),dp=0,dt=0,damage=0,tapped=False,lifelink=False);self.e['free']=e
        self.step('assert_unused_choices',value=0)
        self.step('require_card',ref='source',property='in_hand',value=0)
        if self.accept:self.step('bind_permanent_copy',ref='free',player=self.owner,value=self.rows[self.free_name]['csvid'],index=sum(e['alive'] and e['player']==self.owner and e['name']==self.free_name for ref,e in self.e.items() if ref!='free'))
        self.actions+=self.checks(stack=1)
        self.step('announce_spell',ref='source');self.actions+=self.checks(stack=1)
        if self.mode=='target_removed':self.kill(self.target_ref,True);self.actions+=self.checks(stack=1)
        if self.mode=='countered_parent':
            # Counterspell automatically uses the real current stack parent.
            self.begin('response',response=True);self.step('resolve_response');self.finish('response');self.finish('source',True);self.actions+=self.checks();return
        self.step('resolve');self.finish('source')
        effect=self.p['effect']
        if self.mode=='target_removed':pass
        elif effect=='damage':
            self.e[self.target_ref]['damage']+=4
            if self.e[self.target_ref]['damage']>=self.e[self.target_ref]['toughness']:self.kill(self.target_ref)
        elif effect=='life':self.life[self.owner]+=4
        elif effect=='bounce':
            e=self.e[self.target_ref];e['alive']=False;self.z['hand'][e['player']].append(e['name'])
        elif effect=='team_power':
            for e in self.e.values():
                if e['alive'] and e['creature'] and e['player']==self.owner:e['dp']+=1
        self.actions+=self.checks()
    def primary_combat(self,expiry=False):
        effect=self.p['effect'];attacker='source' if effect=='haste' else 'idle'
        hostile=self.mode=='opponent_attack' or effect=='cannot_block' and self.mode=='own_target'
        if hostile:attacker='enemy'
        actor=1-self.owner if hostile else self.owner
        self.step('turn',player=actor,value=0);self.step('begin_attackers')
        if effect=='haste' and self.accept:self.step('attack',ref='free',value=0)
        self.step('attack',ref=attacker,value=1);self.e[attacker]['tapped']=True
        if self.mode=='two_attackers':self.step('attack',ref='ready_extra',value=1);self.e['ready_extra']['tapped']=True
        self.step('finish_attackers')
        if effect=='exalted' and not hostile and self.e['source']['alive'] and self.mode!='two_attackers':self.e[attacker]['dp']+=1;self.e[attacker]['dt']+=1
        self.actions+=self.checks();self.step('begin_blockers')
        if effect=='cannot_block':self.step('block',ref=self.target_ref,other=attacker,value=int(expiry))
        self.step('advance_combat');self.step('combat_damage')
        if expiry:
            self.e['enemy']['damage']+=self.e[attacker]['power'];self.e[attacker]['damage']+=self.e['enemy']['power']
            self.kill(attacker);self.kill('enemy')
        else:
            self.life[1-actor]-=self.e[attacker]['power']+self.e[attacker]['dp']
            if self.mode=='two_attackers':self.life[1-actor]-=self.e['ready_extra']['power']
        self.step('end_combat');self.actions+=self.checks()
    def case(self):
        from recipes import step
        self.real_cast()
        denied=self.mode in ('source_insufficient_mana','source_wrong_color','no_legal_target')
        if self.mode=='source_removed':self.kill('source',True);self.actions+=self.checks()
        if self.mode=='late_creature':
            pools=copy.deepcopy(self.mana);pools[self.owner]=self.cost('Grizzly Bears');self.set_pools(pools);self.cast('late')
        if not denied and self.mode not in ('countered_parent','target_removed') and self.p['effect'] in ('haste','exalted','cannot_block'):self.primary_combat()
        self.clean()
        if not denied and self.mode not in ('countered_parent','target_removed','own_target') and self.p['effect']=='cannot_block':
            # Real EOT expiration and turn/untap, then existing first-main fixture
            # selection.  This does not claim an automatic upkeep/draw sequence.
            for w in (1-self.owner,self.owner):
                self.step('advance_turn');self.step('untap_step',player=w)
                for e in self.e.values():
                    if e['alive'] and e['player']==w:e['tapped']=False
            self.primary_combat(expiry=True)
        final=self.checks();tail=self.actions+final
        seq=self.pre+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+tail+[step('assert_recorded_state')]
        gaps=['Calibration gate: Cascade decline, scan over land/equal/higher MV, no eligible card and random-bottom contents are held until actual nested-input controls pass.',
                    'No invented intermediate free-child pending window; production nested casting completes during original cast. Extra costs, X/modal/split cards, external copies and countering Cascade separately are not modeled.',
                    'This fixture targets a legal opposing creature; other permanent categories/borrowed ownership, multiple exalted attackers and source removal are staged complements.',
                    'Real cleanup/start-turn/untap plus existing first-main fixture selection does not claim a full automatic upkeep/draw cycle.'] if self.mode=='accept' else [
                    'Random bottom exact contents/multiplicities and unchanged ordered suffix are checked; no guessed permutation or statistical uniformity claim.',
                    'No-eligible/empty library has no planned dialog. A missed real eligible-card offer is exposed by explicit ordinary unused-choice receipt and a missing-child prerequisite, never a strict leftover marker.',
                    'No invented intermediate free-child pending window; additional costs, modal/X/split spells, external copies, independent cast observer, separately countering Cascade and borrowed ownership remain unmodeled.',
                    'Vanilla card_dummy free witnesses are static fixtures, never primaries. Full automatic upkeep/draw sequence is not claimed by the real cleanup/start-turn/untap plus first-main fixture selection.']
        return dict(name=f'CascadeSpellEffectRecipe_{self.csvid}_{self.variant}',card=self.name,fixture='empty_main',seed=123,setup=self.setup,steps=seq,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.d['params'],oracle_text=self.d['oracle_text'],
                    untested_components=gaps))
def expand(name,csvid,d,p,selected,rows,o):
    validate_witnesses(o,rows)
    if len(set(selected))!=len(selected) or set(selected)-set(variants(p)):raise ValueError('unpiloted Cascade variant')
    return [CascadeLedger(name,csvid,d,p,v,rows,o).case() for v in selected]
