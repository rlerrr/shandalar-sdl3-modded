"""Werewolf face abilities driven through actual spell history and upkeep."""

from functools import lru_cache
import json
from pathlib import Path

from library_search_recipes import wave6_exact as exact
from werewolf_spell_history_recipes import Board as HistoryBoard, TRIGGERS


FAMILY = 'werewolf_face_ability_riders'
PROFILE = Path(__file__).parent/'data/werewolf_face_ability_riders_wave8_profiles.json'
RIDER = {
    'fight': (
        ('{T}: This creature deals 2 damage to target creature with flying.',),
        ('{R}, {T}: This creature fights target creature. (Each deals damage equal to its power to the other.)',),
        ('Fight',)),
    'team': (('Attacking creatures you control get +1/+0.',),
             ('Trample','Attacking creatures you control get +3/+0.'),('Trample',)),
    'mana': (('{T}: Add {G}.',),('Vigilance','{T}: Add {G}{G}.'),('Vigilance',)),
    'regen': ((),('{G}: Regenerate this creature.',),('Regenerate',)),
    'pump': (('{1}{G}: This creature gets +2/+2 until end of turn. Activate only once each turn.',),
             ('{3}{G}: This creature gets +4/+4 until end of turn. Activate only once each turn.',),()),
    'recruit': (
        ('{2}{G}: Look at the top three cards of your library. You may reveal a creature card from among them and put it into your hand. Put the rest on the bottom of your library in any order.',),
        ('Creature spells you cast cost {1} less to cast.',),()),
    'hand': (
        ("Sage of Ancient Lore's power and toughness are each equal to the number of cards in your hand.",
         'When this creature enters, draw a card.'),
        ('Vigilance, trample',"Werewolf of Ancient Hunger's power and toughness are each equal to the total number of cards in all players' hands."),
        ('Vigilance','Trample')),
}
COMMON = ('paid_front','front_ready','back_ready','revert_replay')
MODES = {
    'fight': ('front_ping','front_scope','back_fight','back_short','back_target_gone'),
    'team': ('front_team','back_team','foe_attack'),
    'mana': ('fresh_denied','front_mana','back_mana','back_vigilance'),
    'regen': ('front_denied','back_regen','back_short','back_proactive'),
    'pump': ('front_pump','back_pump','back_short','once_reset'),
    'recruit': ('front_reveal','front_decline','back_own_creature','back_noncreature','back_foe_creature'),
    'hand': ('front_play','front_empty_death','back_hand_change','back_combat'),
}
FACE_FIELDS = ('name','oracle_text','mana_cost','type_line','power','toughness','colors')


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(front,back):
    from recipes import COLORS, cost_from_oracle

    if not front or not back or front.get('layout')!='transform' or back.get('layout')!='transform':
        raise ValueError('physical paired transform card required')
    f,b = front['card_faces']
    if (not exact(back['card_faces'],front['card_faces']) or back['oracle_id']!=front['oracle_id']
            or front['engine_face']!=f['name'] or back['engine_face']!=b['name']
            or front['name']!=f['name']+' // '+b['name'] or back['name']!=front['name']
            or not f['mana_cost'] or b['mana_cost'] or b.get('color_indicator')!=b['colors']):
        raise ValueError('complete linked physical faces required')
    texts = [raw['oracle_text'].splitlines() for raw in (f,b)]
    if any(xs[-1:]!=[trigger] for xs,trigger in zip(texts,TRIGGERS)):
        raise ValueError('exact front and back upkeep rules required')
    found = [effect for effect,(left,right,_) in RIDER.items() if [tuple(xs[:-1]) for xs in texts]==[left,right]]
    if len(found)!=1:
        raise ValueError('unsupported whole-face ability rider')
    effect,=found
    keywords = {'Transform',*RIDER[effect][2]}
    if any(set(record['keywords'])!=keywords for record in (front,back)):
        raise ValueError('complete physical keyword set changed')
    sides=[]
    for raw,record,clauses in zip((f,b),(front,back),texts):
        face = record['selected_face']
        if (set(face)!=set(FACE_FIELDS) or any(not exact(face[key],raw.get(key)) for key in FACE_FIELDS)
                or not raw['type_line'].startswith('Creature — ') or 'Werewolf' not in raw['type_line'].split()):
            raise ValueError('selected creature face drift')
        p,t = raw['power'],raw['toughness']
        if effect=='hand':
            if (p,t)!=('*','*'):
                raise ValueError('live hand-based power and toughness required')
            p=t=0  # Model uses actual located hand counts, never this fixed base.
        else:
            if not all(type(n) is str and n.isascii() and n.isdecimal() for n in (p,t)):
                raise ValueError('fixed nonnegative creature characteristics required')
            p,t=int(p),int(t)
        combat_keys=[k for k in ('Vigilance','Trample') if k.lower() in {cl.lower().strip() for line in clauses[:-1] for cl in line.split(',')}]
        sides.append(dict(name=raw['name'],power=p,toughness=t,color=sum(1<<COLORS[c] for c in raw['colors']),
                          keywords=combat_keys,clauses=clauses))
    return dict(effect=effect,price=cost_from_oracle(f),front=sides[0],back=sides[1])


def variants(shape):
    return [f'{mode}_{seat}' for mode in (*COMMON,*MODES[shape['effect']]) for seat in (0,1)]


def spec(declaration,record):
    pin = profiles()['sources'].get((record or {}).get('engine_face'),{})
    if not exact(pin.get('oracle'),record) or not pin:
        raise ValueError('independently pinned full front record required')
    model = classify(record,pin['back_oracle'])
    chosen = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['selected_face']['oracle_text'] or not exact(declaration.get('params'),model)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(model))):
        raise ValueError('whole physical-face model and reviewed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle,rows):
    pin = profiles()
    for name,record in pin['witnesses'].items():
        row = rows.get(name,{})
        if (not exact(record,dict(oracle=oracle.get(name),registry=row)) or row.get('kind')!='card'
                or row.get('handler') in (None,'NULL','0','card_dummy') or oracle[name]['layout']!='normal'):
            raise ValueError(f'implemented Werewolf witness drift: {name}')
    for name,record in pin['sources'].items():
        model = classify(record['oracle'],record['back_oracle'])
        back = model['back']['name']
        for key,oracle_key,identity in [('registry','oracle',name),('back_registry','back_oracle',back)]:
            row = rows.get(identity,{})
            if (not exact(row,record[key]) or not exact(oracle.get(identity),record[oracle_key]) or row.get('kind')!='card'
                    or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented linked Werewolf face drift: {identity}')
        if model['front']['name']!=name or rows[back]['csvid']!=rows[name]['csvid']+1:
            raise ValueError('unreviewed Werewolf physical registry linkage')


class FaceBoard(HistoryBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.effect = shape['effect']
        self.pump = 0
        self.attackers = set()
        super().__init__(name,shape,mode,owner,rows,oracle)
        self.add_rider_witnesses()

    def fog_stock(self):
        counts = [0,0]
        front_aged = self.mode.startswith('front_') and self.mode not in ('front_empty_death','front_play')
        if front_aged or self.mode=='back_hand_change':
            counts[self.foe] = 1
        if self.mode=='revert_replay' or self.mode in ('back_mana','back_vigilance'):
            counts[self.owner] = 2
        elif self.mode=='front_mana':
            counts[self.owner] = 1
        return counts

    def add_rider_witnesses(self):
        m = self.mode
        if m in ('front_ping','front_scope','back_fight','back_short','back_target_gone') and self.effect=='fight':
            self.add('Welkin Tern' if m.startswith('front') else 'Goblin Tinkerer','victim',self.foe)
            if m=='front_scope':
                self.add('Goblin Tinkerer','ground',self.foe)
            if m=='back_target_gone':
                self.add('Unsummon','bounce',self.foe,'hand')
        if self.effect=='team' and m in MODES['team']:
            self.add('Mogg Fanatic','ally',self.owner)
            self.add('Goblin Tinkerer','idle',self.owner)
            self.add('Goblin Tinkerer','blocker',self.foe)
        if self.effect=='regen' and m in ('front_denied','back_regen','back_short'):
            self.add('Murder','removal',self.foe,'hand')
            if m=='back_regen':
                self.add('Shock','wound',self.foe,'hand')
        if m in ('front_reveal','front_decline'):
            for ref in tuple(self.cards):
                if ref.startswith(f'library_{self.owner}_'):
                    del self.cards[ref]
            self.setup = [s for s in self.setup if not s['ref'].startswith(f'library_{self.owner}_')]
            for i,name in enumerate(('Island','Mogg Fanatic','Forest','Goblin Tinkerer')):
                self.add(name,f'look_{i}',self.owner,'library')
        if m in ('back_own_creature','back_foe_creature','back_noncreature'):
            self.add('Incinerate' if m=='back_noncreature' else 'Goblin Tinkerer','discount_test',
                     self.foe if m=='back_foe_creature' else self.owner,'hand')
        if self.effect=='hand':
            if m!='front_empty_death':
                self.add('Plains','stock',self.owner,'hand')
            for i in range(2):
                self.add('Island',f'foe_stock_{i}',self.foe,'hand')
            if m=='back_combat':
                self.add('Goblin Tinkerer','blocker',self.foe)

    def stats(self,ref):
        if self.effect=='hand' and ref=='source':
            hand = sum(c['zone']=='hand' and (self.is_back or c['player']==self.owner) for c in self.cards.values())
            return hand+self.counters+self.pump,hand+self.counters+self.pump
        if ref!='source' and 'power' not in self.oracle[self.cards[ref]['name']]:
            return 0,0
        p,t = super().stats(ref)
        if ref=='source':
            p,t=p+self.pump,t+self.pump
        if (self.effect=='team' and ref in self.attackers and self.cards[ref]['player']==self.owner
                and self.cards['source']['zone']=='battlefield'):
            p += 3 if self.is_back else 1
        return p,t

    def visit_upkeep(self,*,capture=False):
        self.pump=0
        self.attackers.clear()
        super().visit_upkeep(capture=capture)

    def cast_source(self):
        if self.effect!='hand':
            super().cast_source()
            return
        fee = self.shape['price']
        self.fund(self.owner,fee)
        self.emit('cast',ref='source',value=1)
        self.emit('assert_stack',value=1)
        self.pools[self.owner]=[0]*6
        self.observe_mana()
        self.emit('resolve')
        source = self.cards['source']
        source['zone'] = 'graveyard' if self.mode=='front_empty_death' else 'battlefield'
        self.cards[f'library_{self.owner}_0']['zone']='hand'
        self.sick = True
        self.history[self.owner]+=1
        self.receipts()

    def prepare(self):
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,[0]*6)
        self.cast_source()
        m = self.mode
        if m in ('paid_front','fresh_denied','front_play','front_empty_death'):
            return
        self.visit_upkeep()
        if m.startswith('front_'):
            self.fog(self.foe)
        self.visit_upkeep()

    def begin_ability(self,fee,*,target=None,allowed=True,tap=False,budget=None):
        self.fund(self.owner,fee if budget is None else budget)
        self.emit('can_activate',ref='source',value=int(allowed))
        if allowed and target:
            self.emit('choose_target',ref=target,player=self.owner)
        self.emit('activate',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pools[self.owner]=[a-b for a,b in zip(self.pools[self.owner],fee)]
            self.cards['source']['tapped'] |= tap
        self.emit('assert_card',ref='source',property='tapped',value=int(self.cards['source']['tapped']))
        self.observe_mana()

    def resolve_plain(self):
        self.emit('resolve')
        self.emit('refresh')

    def mark(self,ref,damage):
        self.cards[ref]['damage'] += damage
        if self.cards[ref]['damage']>=self.stats(ref)[1]:
            self.cards[ref]['zone']='graveyard'

    def fight_rider(self):
        m = self.mode
        if m=='front_scope':
            for ref,value in [('victim',1),('ground',0)]:
                self.emit('probe_activation_target',ref='source',other=ref,index=-1,value=value)
        fee = [0,0,0,0,int(self.is_back),0]
        allowed = m!='back_short'
        self.begin_ability(fee,target='victim',tap=True,allowed=allowed,budget=None if allowed else [0]*6)
        if not allowed:
            self.receipts()
            return
        if m=='back_target_gone':
            from recipes import cost_from_oracle

            response_fee = cost_from_oracle(self.oracle['Unsummon'])
            self.fund(self.foe,response_fee)
            self.emit('choose_target',player=self.foe,ref='victim')
            self.emit('cast',ref='bounce',value=1)
            self.emit('assert_stack',value=2)
            self.pools[self.foe]=[0]*6
            self.resolve_plain()
            self.cards['bounce']['zone']='graveyard'
            self.cards['victim']['zone']='hand'
        else:
            self.resolve_plain()
            if self.is_back:
                opponent = self.stats('victim')[0]
                self.mark('victim',self.stats('source')[0])
                self.mark('source',opponent)
            else:
                self.mark('victim',2)
        self.receipts()

    def combat_rider(self,attackers,block=None):
        self.emit('begin_attackers')
        for ref in attackers:
            self.emit('attack',ref=ref,value=1)
            self.cards[ref]['tapped'] = ref!='source' or 'Vigilance' not in self.face()['keywords']
        self.attackers=set(attackers)
        self.emit('finish_attackers')
        self.receipts()
        self.emit('begin_blockers')
        if block:
            ref,blocker=block
            self.emit('block',ref=blocker,other=ref,value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        for ref in attackers:
            p,t=self.stats(ref)
            if block and ref==block[0]:
                blocker=block[1]
                bp,bt=self.stats(blocker)
                self.mark(blocker,p)
                self.mark(ref,bp)
                if ref=='source' and 'Trample' in self.face()['keywords']:
                    self.life[1-self.current]-=max(0,p-bt)
            else:
                self.life[1-self.current]-=p
        self.attackers.clear()
        self.receipts()

    def mana_rider(self):
        if self.mode=='fresh_denied':
            self.emit('can_activate',ref='source',value=0)
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        if self.mode=='back_vigilance':
            self.combat_rider(('source',))
        amount = 2 if self.is_back else 1
        self.emit('can_activate',ref='source',value=1)
        self.emit('activate',ref='source',value=1)
        self.pools[self.owner][3]=amount
        self.cards['source']['tapped']=True
        self.receipts()
        self.emit('can_activate',ref='source',value=0)
        for _ in range(amount):
            ref=f'fog_{self.owner}_{self.next_spell[self.owner]}'
            self.next_spell[self.owner]+=1
            self.emit('cast',ref=ref,value=1)
            self.emit('assert_stack',value=1)
            self.pools[self.owner][3]-=1
            self.observe_mana()
            self.resolve_plain()
            self.cards[ref]['zone']='graveyard'
            self.history[self.owner]+=1
        self.receipts()
        self.visit_upkeep()

    def regeneration(self):
        m = self.mode
        green = [0,0,0,1,0,0]
        if m=='back_proactive':
            self.begin_ability(green)
            self.resolve_plain()
            self.receipts()
            return
        if m=='back_regen':
            self.spell('wound',target='source')
            self.resolve_plain()
            self.cards['wound']['zone']='graveyard'
            self.cards['source']['damage']=2
            self.receipts()
        save = m=='back_regen'
        self.fund(self.owner,green if m!='back_short' else [0]*6)
        self.spell('removal',target='source')
        self.emit('choose_regeneration',ref='source')
        self.emit('resolve')
        self.emit('assert_unused_choices',value=int(not save))
        self.cards['removal']['zone']='graveyard'
        if save:
            self.pools[self.owner][3]=0
            self.cards['source'].update(tapped=True,damage=0)
        else:
            self.cards['source']['zone']='graveyard'
        self.receipts()

    def shade(self):
        m=self.mode
        fee=[3 if self.is_back else 1,0,0,1,0,0]
        if m=='back_short':
            self.begin_ability(fee,allowed=False,budget=[3,0,0,0,0,0])
            self.receipts()
            return
        delta=4 if self.is_back else 2
        self.begin_ability(fee)
        self.resolve_plain()
        self.pump=delta
        self.receipts()
        if m=='once_reset':
            self.visit_upkeep()
            self.begin_ability(fee)
            self.resolve_plain()
            self.pump=delta
        else:
            self.begin_ability(fee,allowed=False)
        self.receipts()

    def recruit(self):
        decline = self.mode=='front_decline'
        self.begin_ability([2,0,0,1,0,0])
        selected = self.rows['Mogg Fanatic']['csvid']
        if decline:
            self.emit('choose_optional_zone',player=self.owner,value=selected)
            bottom = ('look_2','look_1','look_0')
        else:
            self.emit('choose_zone_card',player=self.owner,value=selected)
            if self.owner==0:
                self.emit('choose_dialog',player=0,value=0)
            self.cards['look_1']['zone']='hand'
            bottom = ('look_2','look_0')
        for ref in bottom[:-1]:
            self.emit('choose_zone_card',player=self.owner,value=self.identity(self.cards[ref]))
        self.emit('resolve')
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_forced_zone_choices',value=0)
        for ref in bottom:
            self.cards[ref]=self.cards.pop(ref)
        self.receipts()

    def discounts(self):
        m=self.mode
        actor=self.cards['discount_test']['player']
        if actor==self.foe:
            self.visit_upkeep()
        self.fund(actor,[0,0,0,0,1,0])
        if m=='back_noncreature':
            self.emit('choose_player',player=actor,value=1-actor)
        allowed=m=='back_own_creature'
        self.emit('cast',ref='discount_test',value=int(allowed))
        if m=='back_noncreature':
            self.emit('assert_unused_choices',value=1)
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pools[actor][4]=0
            self.observe_mana()
            self.resolve_plain()
            self.cards['discount_test']['zone']='battlefield'
        self.receipts() if allowed or m!='back_noncreature' else self.observe_mana()

    def hand_rider(self):
        if self.mode=='front_play':
            self.emit('play_land',ref='stock',value=1)
            self.cards['stock']['zone']='battlefield'
            self.receipts()
        elif self.mode=='back_hand_change':
            self.fog(self.foe)
            self.emit('play_land',ref='stock',value=1)
            self.cards['stock']['zone']='battlefield'
            self.receipts()
        elif self.mode=='back_combat':
            self.combat_rider(('source',),('source','blocker'))

    def run(self):
        self.prepare()
        m,e=self.mode,self.effect
        if m in COMMON:
            if m=='revert_replay':
                self.fog(self.owner)
                self.fog(self.owner)
                self.visit_upkeep(capture=True)
            return
        if e=='fight':
            self.fight_rider()
        elif e=='team':
            if m=='foe_attack':
                self.visit_upkeep()
                self.combat_rider(('blocker',))
            else:
                self.combat_rider(('source','ally'),('source','blocker') if self.is_back else None)
        elif e=='mana':
            self.mana_rider()
        elif e=='regen':
            self.regeneration()
        elif e=='pump':
            self.shade()
        elif e=='recruit':
            self.recruit() if m.startswith('front_') else self.discounts()
        else:
            self.hand_rider()


EFFECT_GAPS = {
    'fight': ['Simultaneous fight damage replacement and source removal before resolution'],
    'team': ['Controlled attacking creatures changing controllers during combat'],
    'regen': ['Destroy-without-regeneration and sacrifice interactions'],
    'recruit': ['Independent reveal-presentation buffer', 'Multiple cost reducers in the same game'],
    'hand': ['Control transfer during a hand-size change'],
}


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or name not in profiles()['sources'] or set(selected)-set(variants(shape)):
        raise ValueError('reserved physical Werewolf fronts and scoped variants required')
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        world=FaceBoard(name,shape,mode,int(seat),rows,oracle)
        world.run()
        steps=world.steps
        if world.snapshot is not None:
            cut=world.snapshot
            suffix=steps[cut:]
            steps=steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        meta = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                    covered_clauses=[*shape['front']['clauses'],*shape['back']['clauses']],
                    untested_components=['Multiplayer','Responses inside the synchronous transform upkeep callback',*EFFECT_GAPS.get(shape['effect'],[])])
        cases.append(dict(name=f'WerewolfFaceAbilityRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2047,
                          setup=world.setup,steps=steps,_recipe=meta))
    return cases
