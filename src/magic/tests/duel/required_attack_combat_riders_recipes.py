"""Required attack declaration composed with audited, actual combat riders."""
import copy
import json
from pathlib import Path

from library_search_recipes import wave6_exact
from required_attack_creature_recipes import CAST, CLAUSE, NEXT_TURN, RequiredAttackCase


FAMILY = 'required_attack_combat_riders'
PROFILE = Path(__file__).parent / 'data/required_attack_combat_riders_wave8_profiles.json'
GRAMMAR = {
    ('Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.)',):
        ('ronin',('Bushido',),'Creature — Human Samurai'),
    ('Whenever this creature deals combat damage to a player, put a +1/+1 counter on it.',):
        ('neonate',(),'Creature — Vampire'),
    ('Shadow (This creature can block or be blocked by only creatures with shadow.)',):
        ('slayer',('Shadow',),'Creature — Dauthi Soldier'),
    ("This creature doesn't untap during your untap step.", 'Whenever another creature dies, untap this creature.'):
        ('galvanic',(),'Artifact Creature — Juggernaut'),
    ('Indestructible',"Darksteel Juggernaut's power and toughness are each equal to the number of artifacts you control."):
        ('darksteel',('Indestructible',),'Artifact Creature — Juggernaut'),
    ('This creature gets +2/+0 as long as you control another artifact.',):
        ('ramroller',(),'Artifact Creature — Juggernaut'),
    ('Prowess (Whenever you cast a noncreature spell, this creature gets +1/+1 until end of turn.)',):
        ('bully',('Prowess',),'Creature — Human Warrior'),
    ('Flying, trample','Whenever this creature deals damage, you lose that much life.'):
        ('caliph',('Flying','Trample'),'Creature — Djinn'),
}
HANDLERS = {'ronin':'manalinkeh_card_battle_mad_ronin','neonate':'manalinkeh_card_bloodcrazed_neonate',
            'slayer':'manalinkeh_card_dauthi_slayer','galvanic':'manalinkeh_card_galvanic_juggernaut',
            'darksteel':'manalinkeh_card_darksteel_juggernaut','ramroller':'manalinkeh_card_ramroller',
            'bully':'manalinkeh_card_mage_ring_bully','caliph':'manalinkeh_card_emberwilde_caliph'}
COMMON = ('omitted','tapped','sick','pacified','opponent_turn','competing_selected','two_required',
          'paid_fresh','paid_next_turn','unfunded','proposed_replay')
ARTIFACT = ('own_artifacts','opponent_artifacts','other_zones','gain_artifact','lose_artifact')
EXTRAS = {
    'ronin':('blocked_bushido','defend_bushido','bushido_cleanup'),
    'neonate':('player_growth','repeat_hit','creature_only','walker_damage','noncombat'),
    'slayer':('ground_evasion','shadow_exchange','defend_ground','defend_shadow'),
    'galvanic':('next_locked','own_death','foreign_death','artifact_death','bounced_no_death'),
    'darksteel':ARTIFACT+('indestructible_destroy','indestructible_combat','exiled'),
    'ramroller':ARTIFACT,
    'bully':('own_noncreature','double_noncreature','opponent_noncreature','own_creature','prowess_cleanup','countered_spell'),
    'caliph':('trample_split','flying_ground','walker_damage','noncombat','fogged'),
}
DEFENDING={'defend_bushido','defend_ground','defend_shadow'}
REENTRY={'repeat_hit','next_locked','own_death','foreign_death','artifact_death','bounced_no_death'}
WITNESSES={'Forest','Grizzly Bears','Island','Mountain','Hill Giant','Plains','Pacifism',
           'Dazzling Ramparts','Soltari Foot Soldier','Jace Beleren','Expedition Map','Darksteel Relic',
           'Lightning Bolt','Disenchant','Unsummon','Murder','Swords to Plowshares','Think Twice',
           'Counterspell','Fog','Wind Drake','Hermetic Study'}
DUMMIES={'Grizzly Bears','Hill Giant','Wind Drake','Soltari Foot Soldier'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle
    if not record or record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces'):
        raise ValueError('whole normal required attacker needed')
    lines=record['oracle_text'].splitlines()
    riders=tuple(line for line in lines if line!=CLAUSE)
    rule=GRAMMAR.get(riders)
    if not rule or lines.count(CLAUSE)!=1:
        raise ValueError('exact requirement and full reviewed printed riders needed')
    mode,keys,type_line=rule
    if record['type_line']!=type_line or sorted(record['keywords'])!=sorted(keys):
        raise ValueError('printed type and keyword mismatch')
    stats=[record.get(field) for field in ('power','toughness')]
    if mode=='darksteel':
        if stats!=['*','*']:
            raise ValueError('true characteristic-defining artifact count required')
        power=toughness=None
    else:
        if any(type(x) is not str or not x.isascii() or not x.isdecimal() for x in stats):
            raise ValueError('whole fixed printed stats required')
        power,toughness=map(int,stats)
    return dict(mode=mode,costs={'printed':cost_from_oracle(record)},power=power,toughness=toughness,keywords=list(keys))


def variants(shape):
    modes=COMMON+EXTRAS[shape['mode']]
    if any(shape['costs']['printed'][1:]):
        modes+=('wrong_color',)
    return [f'{mode}_{owner}' for mode in modes for owner in (0,1)]


def spec(declaration,record):
    shape=classify(record)
    pin=profiles()['sources'].get(record['name'])
    chosen=declaration.get('variants')
    if (not pin or not wave6_exact(pin['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or not isinstance(chosen,list) or not chosen or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('pinned complete record, exact requirements and requested cases needed')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if set(data)!={'sources','witnesses'} or set(data['witnesses'])!=WITNESSES:
        raise ValueError('complete typed combat, spell and eligibility witnesses required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row=rows[name]
            if (not wave6_exact(oracle[name],pin['oracle']) or not wave6_exact(row,pin['registry'])
                    or row['kind']!='card' or row['handler'] in (None,'NULL','0')
                    or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError('changed source or witness: '+name)
            if group=='sources' and row['handler']!=HANDLERS[classify(oracle[name])['mode']]:
                raise ValueError('changed reviewed rider handler: '+name)


class Rider(RequiredAttackCase):
    def __init__(self,name,csvid,declaration,shape,variant,rows,oracle):
        self.shape=shape
        self.rider=shape['mode']
        self.loyalty=None
        super().__init__(name,csvid,declaration,shape,variant,rows,oracle)
        self.configure()
        self.names=sorted({entry['name'] for entry in self.setup})

    def add(self,name,ref,who,zone='battlefield',state=None):
        from recipes import card

        entry=card(name,ref,who,zone)
        if state:
            entry['state']=state
        self.setup.append(entry)
        self.zones[zone][who].append(name)
        if zone in ('battlefield','hand'):
            oracle=self.oracle[name]
            permanent_type=oracle['type_line'].partition(' — ')[0].split()
            values=[oracle.get(prop) for prop in ('power','toughness')]
            stats=None
            if 'Creature' in permanent_type:
                stats=tuple(int(item) if item!='*' else 0 for item in values)
            self.entities[ref]=dict(name=name,who=who,zone=zone,printed=stats,stats=stats,tapped=bool(state and 'tapped' in state),
                                     damage=0,counters=0,bonus=0,artifact='Artifact' in permanent_type)

    def configure(self):
        mode, owner, other=self.mode,self.owner,self.other
        if mode in DEFENDING:
            self.active=other
            for action in self.before:
                if action['op']=='turn':
                    action['player']=other
            self.add('Soltari Foot Soldier' if mode=='defend_shadow' else 'Grizzly Bears',
                     'ally',other)
        blockers={'blocked_bushido':'Grizzly Bears','bushido_cleanup':'Grizzly Bears','creature_only':'Dazzling Ramparts',
                  'ground_evasion':'Hill Giant','shadow_exchange':'Soltari Foot Soldier',
                  'indestructible_combat':'Grizzly Bears','trample_split':'Wind Drake','flying_ground':'Hill Giant'}
        if mode in blockers:
            self.add(blockers[mode],'blocker',other)
        if mode=='walker_damage':
            self.add('Jace Beleren','walker',other)
            self.loyalty=int(self.oracle['Jace Beleren']['loyalty'])
        if mode=='noncombat':
            self.add('Hermetic Study','spell',owner,'hand')
        if mode=='fogged':
            self.add('Fog','spell',other,'hand')
        if mode in ('own_death','foreign_death','artifact_death','bounced_no_death'):
            target='Expedition Map' if mode=='artifact_death' else 'Grizzly Bears'
            self.add(target,'victim',other if mode=='foreign_death' else owner)
            spell={'artifact_death':'Disenchant','bounced_no_death':'Unsummon'}.get(mode,'Lightning Bolt')
            self.add(spell,'spell',other,'hand')
        self.artifact_fixtures()
        if mode in ('own_noncreature','opponent_noncreature','double_noncreature','prowess_cleanup','countered_spell'):
            self.add('Think Twice','spell',other if mode=='opponent_noncreature' else owner,'hand')
            if mode=='double_noncreature':
                self.add('Think Twice','second_spell',owner,'hand')
            if mode=='countered_spell':
                self.add('Counterspell','counter',other,'hand')
        if mode=='own_creature':
            self.add('Grizzly Bears','spell',owner,'hand')

    def artifact_fixtures(self):
        mode,owner,other=self.mode,self.owner,self.other
        if mode in ('own_artifacts','opponent_artifacts','lose_artifact'):
            self.add('Expedition Map','artifact',other if mode=='opponent_artifacts' else owner)
            if mode=='own_artifacts':
                self.add('Darksteel Relic','second_artifact',owner)
        if mode=='gain_artifact':
            self.add('Expedition Map','spell',owner,'hand')
        if mode=='lose_artifact':
            self.add('Disenchant','spell',other,'hand')
        if mode=='other_zones':
            for zone in ('hand','graveyard','exile'):
                self.add('Expedition Map',zone+'_artifact',owner,zone)
        if mode in ('indestructible_destroy','exiled'):
            self.add('Murder' if mode=='indestructible_destroy' else 'Swords to Plowshares','spell',other,'hand')

    def move(self,ref,zone):
        resolving_pacifism = (self.rider=='bully' and ref=='aura' and self.entities[ref]['zone']=='stack'
                              and zone=='battlefield')
        super().move(ref,zone)
        if resolving_pacifism:
            # The actual Pacifism cast triggers Prowess. The native
            # resolution boundary settles that earlier cast trigger too.
            self.entities['source']['bonus']+=1

    def update_stats(self):
        for card in self.entities.values():
            if card['printed'] is not None:
                count=card['counters']+card['bonus']
                card['stats']=tuple(item+count for item in card['printed'])
        if self.rider in ('ramroller','darksteel'):
            count=sum(card['who']==self.owner and card['artifact'] and card['zone']=='battlefield'
                      for card in self.entities.values())
            for card in self.entities.values():
                if card['name']==self.name:
                    card['stats']=(count,count) if self.rider=='darksteel' else (2+2*int(count>1),3)

    def receipt(self,stack=0):
        self.update_stats()
        super().receipt(stack)
        if self.entities['source']['zone']=='battlefield':
            self.observe('source','plus1_counters',self.entities['source']['counters'])
            if 'Artifact' in self.oracle[self.name]['type_line'].partition(' — ')[0].split():
                self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='artifact',value=1)
        if self.loyalty:
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.loyalty)

    def fund(self,actor,price):
        self.mana[actor]=list(price)
        for i,n in enumerate(price):
            self.emit('set_mana',player=actor,color=i,value=n)
        self.emit('refresh')

    def spell(self,ref,*,target=None,zone='graveyard',where=None,draw=False,bonus=False,untap_source=False):
        from recipes import cost_from_oracle

        spell=self.entities[ref]
        actor=spell['who']
        self.fund(actor,cost_from_oracle(self.oracle[spell['name']]))
        if target:
            self.emit('choose_target',ref=target,player=actor)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.mana[actor]=[0]*6
        self.emit('resolve')
        self.move(ref,zone)
        if where is not None:
            self.move(target,where)
        if draw:
            self.zones['hand'][actor].append(self.zones['library'][actor].pop(0))
        if bonus:
            self.entities['source']['bonus']+=1
        if untap_source:
            self.entities['source']['tapped']=False
        self.emit('refresh')
        self.receipt()

    def countered_prowess(self):
        from recipes import cost_from_oracle

        self.fund(self.owner,cost_from_oracle(self.oracle['Think Twice']))
        self.emit('cast',ref='spell',value=1)
        self.emit('assert_stack',value=1)
        self.mana[self.owner]=[0]*6
        self.emit('announce_spell',ref='spell')
        self.fund(self.other,cost_from_oracle(self.oracle['Counterspell']))
        self.emit('cast_response',ref='counter',value=1)
        self.emit('assert_stack',value=2)
        self.mana[self.other]=[0]*6
        self.emit('resolve_response')
        self.move('counter','graveyard')
        self.move('spell','graveyard')
        self.entities['source']['bonus']+=1
        self.emit('refresh')
        self.receipt()

    def noncombat(self):
        self.spell('spell',target='source',zone='battlefield')
        self.emit('require_can_activate',ref='spell',value=1)
        self.emit('choose_player',player=self.owner,value=self.other)
        self.emit('activate',ref='spell',value=1)
        self.entities['source']['tapped']=True
        self.emit('assert_stack',value=1)
        self.emit('resolve')
        self.life[self.other]-=1
        if self.rider=='caliph':
            self.life[self.owner]-=1
        self.receipt()

    def attack(self,ref,*,allowed=True):
        if self.mode=='walker_damage' and ref=='source':
            self.emit('choose_dialog',player=self.owner,value=1)
            self.emit('attack_with_tax',ref=ref,value=int(allowed))
            self.observe(ref,'attacking',allowed)
            self.entities[ref]['tapped']=allowed
        else:
            super().attack(ref,allowed=allowed)

    def declare(self):
        if (self.mode!='tapped' and self.active==self.owner and self.entities['source']['tapped']
                and self.entities['source']['zone']=='battlefield'):
            self.emit('begin_required_attackers')
            self.attack('source',allowed=False)
            self.emit('can_finish_attackers',value=1)
            self.emit('finish_attackers')
            self.receipt()
            return []
        if self.mode not in DEFENDING and self.entities['source']['zone']=='battlefield':
            return super().declare()
        self.emit('begin_required_attackers')
        self.emit('can_finish_attackers',value=1)
        if self.mode in DEFENDING:
            self.attack('ally')
        self.emit('can_finish_attackers',value=1)
        self.emit('finish_attackers')
        self.receipt()
        return ['ally'] if self.mode in DEFENDING else []

    def combat(self,attackers):
        mode=self.mode
        blocked=len(attackers)==1 and ('blocker' in self.entities or mode in DEFENDING)
        attacker=attackers[0] if blocked else None
        blocker='source' if mode in DEFENDING else 'blocker'
        attempted=blocked
        blocked=blocked and mode not in ('defend_ground','ground_evasion','flying_ground')
        self.emit('begin_blockers')
        if attempted:
            self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        for ref in attackers:
            self.observe(ref,'blocked',blocked)
        if blocked and self.rider=='ronin':
            self.entities['source']['bonus']+=2
        self.receipt()
        self.emit('combat_damage')
        if mode!='fogged':
            if blocked:
                self.exchange(attacker,blocker)
            else:
                for ref in attackers:
                    self.combat_hit(ref)
        self.emit('resolve')
        self.emit('refresh')
        self.receipt()
        self.emit('end_combat')

    def exchange(self,attacker,blocker):
        a,b=self.entities[attacker],self.entities[blocker]
        outgoing=a['stats'][0]
        incoming=b['stats'][0]
        spill=max(0,outgoing-b['stats'][1]) if self.rider=='caliph' and attacker=='source' else 0
        self.life[1-self.active]-=spill
        if self.rider=='caliph' and attacker=='source':
            self.life[self.owner]-=outgoing
        a['damage']+=incoming
        b['damage']+=outgoing-spill
        for ref,entity in ((attacker,a),(blocker,b)):
            if entity['damage']>=entity['stats'][1] and not (self.rider=='darksteel' and entity['name']==self.name):
                self.move(ref,'graveyard')

    def combat_hit(self,ref):
        creature=self.entities[ref]
        power=creature['stats'][0]
        source=creature['name']==self.name
        if self.mode=='walker_damage' and ref=='source':
            self.loyalty=max(0,self.loyalty-power)
            if not self.loyalty:
                self.move('walker','graveyard')
        else:
            self.life[1-self.active]-=power
            if source and self.rider=='neonate' and power>0:
                creature['counters']+=1
        if source and self.rider=='caliph':
            self.life[self.owner]-=power

    def next_own_turn(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('untap_step',player=self.owner)
        self.emit('turn',player=self.owner,value=0)
        for ref,entity in self.entities.items():
            entity.update(bonus=0,damage=0)
            if entity['zone']=='battlefield' and entity['who']==self.owner and not (ref=='source' and self.rider=='galvanic'):
                entity['tapped']=False
        self.receipt()

    def before_combat(self):
        mode=self.mode
        if mode=='pacified':
            self.emit('choose_target',ref='source',player=self.owner)
            self.cast('aura',self.aura_fee)
        elif mode=='gain_artifact':
            self.spell('spell',zone='battlefield')
        elif mode=='lose_artifact':
            self.spell('spell',target='artifact',where='graveyard')
        elif mode=='indestructible_destroy':
            self.spell('spell',target='source')
        elif mode=='exiled':
            self.update_stats()
            self.life[self.owner]+=self.entities['source']['stats'][0]
            self.spell('spell',target='source',where='exile')
        elif mode in ('own_noncreature','double_noncreature','opponent_noncreature','prowess_cleanup'):
            self.spell('spell',draw=True,bonus=mode!='opponent_noncreature')
            if mode=='double_noncreature':
                self.spell('second_spell',draw=True,bonus=True)
        elif mode=='own_creature':
            self.spell('spell',zone='battlefield')
        elif mode=='countered_spell':
            self.countered_prowess()
        elif mode=='fogged':
            self.spell('spell')
        elif mode=='noncombat':
            self.noncombat()

    def after_combat(self):
        mode=self.mode
        if mode in ('bushido_cleanup','prowess_cleanup'):
            self.emit('cleanup')
            for card in self.entities.values():
                card.update(bonus=0,damage=0)
            self.emit('refresh')
            self.receipt()
        if mode in REENTRY:
            if mode in ('own_death','foreign_death','artifact_death','bounced_no_death'):
                self.spell('spell',target='victim',where='hand' if mode=='bounced_no_death' else 'graveyard',
                           untap_source=mode in ('own_death','foreign_death'))
            self.next_own_turn()
            self.combat(self.declare())


def build(name,csvid,declaration,shape,variant,rows,oracle):
    from recipes import step

    world=Rider(name,csvid,declaration,shape,variant,rows,oracle)
    world.receipt()
    if world.mode in CAST:
        world.cast('source',world.fee,denied=world.mode in ('unfunded','wrong_color'))
        if world.mode in NEXT_TURN:
            world.next_own_turn()
    if world.mode not in ('unfunded','wrong_color'):
        world.before_combat()
        world.combat(world.declare())
        world.after_combat()
    steps=world.before+world.steps
    if world.mode=='proposed_replay':
        cut=len(world.before)+world.snapshot
        suffix=steps[cut:]
        steps=steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(suffix)+[step('assert_recorded_state')]
    gaps=['Goad competing requirements, multiplayer attack assignments, and independent responses inside the native attack-declaration callback.']
    return dict(name=f'RequiredAttackCombatRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                seed=123,setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=gaps))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('exact approved distinct required-attack scenarios required')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
