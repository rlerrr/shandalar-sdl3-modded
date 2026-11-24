"""Whole-card infect riders through real paid casts and combat damage."""

import copy
import json
from pathlib import Path

from counted_battlefield_damage_recipes import Game
from counter_damage_creature_recipes import MECHANICS
from library_search_recipes import wave6_exact


FAMILY = 'infect_combat_riders'
PROFILE = Path(__file__).parent / 'data/infect_combat_riders_wave8_profiles.json'
INFECT = 'Infect '+MECHANICS['Infect']
SHAPES = {
    "This creature can't be blocked.": ('unblockable','latch_seeker'),
    "This creature can't attack unless defending player is poisoned.": ('poison_gate','chained_throatseeker'),
    'When this creature enters, each player gets a poison counter.': ('entry_poison','ichor_rats'),
    'Whenever this creature becomes blocked, it gets +2/+2 until end of turn.': ('blocked_pump','ichorclaw_myr'),
    'Whenever this creature attacks, if defending player is poisoned, it gets +1/+1 until end of turn.':
        ('poison_pump','septic_rats'),
    "Whenever a source deals damage to this creature, that source's controller gets a poison counter.":
        ('received_poison','reaper_of_sheoldred'),
}
CASES = {
    'unblockable': (),
    'poison_gate': ('unpoisoned','self_only'),
    'entry_poison': ('countered','recast'),
    'blocked_pump': ('cleanup',),
    'poison_pump': ('foe_poisoned','self_only','poisoned_block'),
    'received_poison': ('enemy_spell','friendly_spell','mixed_sources','other_creature'),
}
COMMON = ('short','fresh','unblocked','blocked','defending','fogged','replay')
WITNESSES = {'Dazzling Ramparts','Hill Giant','Glistener Elf','Counterspell','Unsummon','Shock','Fog'}
DUMMIES = {'Hill Giant','Glistener Elf'}


FOLLOW = {
    "You may have this creature assign its combat damage as though it weren't blocked.":
        ('bypass','thorn_elemental',['Infect']),
    'At the beginning of your upkeep, you get a poison counter.':
        ('upkeep_self','phyrexian_vatmother',['Infect']),
    'Whenever a creature dealt damage by Rot Wolf this turn dies, you may draw a card.':
        ('rot_draw','rot_wolf',['Infect']),
    '{1}{G}: Regenerate this creature.':
        ('regenerate','gorilla_chieftain',['Regenerate','Infect']),
    '{B}: Regenerate this creature.':
        ('regenerate','drudge_skeletons',['Regenerate','Infect']),
    'Other creatures you control with infect get +1/+1.\n'
    'Whenever you cast a creature spell with infect, target player gets a poison counter.':
        ('infect_lord','hand_of_the_praetors',['Infect']),
    'Whenever this creature deals combat damage to a player, you may sacrifice it. If you do, '
    'that player discards a card for each poison counter they have.':
        ('specter','whispering_specter',['Flying','Infect']),
    'At the beginning of your upkeep, create a 1/1 green Phyrexian Insect creature token with infect for each poison counter your opponents have.':
        ('swarm','phyrexian_swarmlord',['Infect']),
}
FOLLOW_BASIC = ('short','unblocked','blocked','defending','replay')
FOLLOW_CASES = {
    'bypass': ('bypass','normal'),
    'upkeep_self': ('upkeep_each','upkeep_removed'),
    'rot_draw': ('wolf_draw','wolf_decline','wolf_next_turn','wolf_unrelated'),
    'regenerate': ('regen_paid','regen_short','regen_proactive'),
    'infect_lord': ('lord_scope','cast_enemy_poison','cast_self_poison','infect_countered','noninfect'),
    'specter': ('discard_two','decline','short_hand','flying_blocked'),
    'swarm': ('swarm_scale','swarm_self_only','swarm_removed'),
}
WITNESSES |= {'Grizzly Bears','Forest','Island','Wind Drake','Insect','Giant Growth'}
DUMMIES |= {'Grizzly Bears','Wind Drake','Forest','Island'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def complete_shape(record):
    lines=record.get('oracle_text','').splitlines()
    if lines[:1]==['Flying']:
        lines=lines[1:]
    if len(lines)<2 or lines[0]!=INFECT:
        raise ValueError('complete infect rules line required')
    effect='\n'.join(lines[1:])
    if effect in SHAPES:
        return (*SHAPES[effect],['Infect'])
    if effect not in FOLLOW:
        raise ValueError('whole reviewed infect rider required')
    return FOLLOW[effect]


def classify(record):
    from recipes import cost_from_oracle

    record=record or {}
    printed=record.get('type_line','').partition(' — ')[0]
    if (record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces')
            or printed not in ('Creature','Artifact Creature')):
        raise ValueError('complete normal infect creature required')
    rider,_,keys=complete_shape(record)
    if record['keywords']!=keys or ('Flying' in keys)!=record['oracle_text'].startswith('Flying\n'):
        raise ValueError('printed infect and optional Flying keywords must be exact')
    stats=dict(rider=rider,mana=cost_from_oracle(record),power=int(record['power']),
               toughness=int(record['toughness']),artifact=printed=='Artifact Creature')
    if rider=='regenerate':
        fee=record['oracle_text'].splitlines()[-1].split(':',1)[0]
        stats['regen']=cost_from_oracle({'mana_cost':fee})
    return stats


def handler(record):
    return 'manalinkeh_card_'+complete_shape(record)[1]


def variants(shape):
    rider=shape['rider']
    modes=COMMON+CASES[rider] if rider in CASES else FOLLOW_BASIC+FOLLOW_CASES[rider]
    return [f'{scene}_{seat}' for scene in modes for seat in (0,1)]


def spec(declaration,record):
    shape=classify(record)
    pin=profiles()['sources'].get(record['name'])
    chosen=declaration.get('variants')
    if (not pin or not wave6_exact(pin['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('pinned complete infect and rider declaration required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profiles()
    if data.get('version')!=1 or not data.get('sources') or set(data.get('witnesses',()))!=WITNESSES:
        raise ValueError('complete reviewed infect witnesses required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row,record=rows[name],oracle[name]
            if (not wave6_exact(pin,dict(oracle=record,registry=row)) or row['kind']!='card' or record['layout']!=('token' if name=='Insect' else 'normal')
                    or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError(f'changed reviewed infect source or witness: {name}')
            if group=='sources' and row['handler']!=handler(record):
                raise ValueError(f'changed reviewed infect rider handler: {name}')
    if rows['Insect']['csvid']!=18770 or rows['Insect']['handler']!='manalinkeh_card_insect':
        raise ValueError('engine Insect template identity changed')
    if oracle['Glistener Elf']['oracle_text']!=INFECT:
        raise ValueError('independent one-poison source required')


class InfectBoard(Game):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.active=owner
        self.life=[20,20]
        self.poison=[0,0]
        self.rider=shape['rider']
        self.minus={}
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode,owner,foe=self.mode,self.owner,self.foe
        if mode in ('blocked','cleanup','poisoned_block','replay'):
            self.add('Dazzling Ramparts','blocker',foe)
        if mode=='defending' or (mode=='fogged' and self.rider=='received_poison'):
            self.add('Hill Giant','attacker',foe)
        if self.rider=='poison_gate' and mode in ('unblocked','blocked','fogged','replay') or mode in ('foe_poisoned','poisoned_block'):
            self.add('Glistener Elf','primer',owner)
        if mode=='self_only':
            self.add('Glistener Elf','primer',foe)
        if mode=='fogged':
            self.add('Fog','fog',foe,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',foe,'hand')
        if mode=='recast':
            self.add('Unsummon','return_spell',foe,'hand')
        if mode in ('enemy_spell','mixed_sources','other_creature'):
            self.add('Shock','enemy_spell',foe,'hand')
        if mode in ('friendly_spell','mixed_sources'):
            self.add('Shock','friendly_spell',owner,'hand')
        if mode=='other_creature':
            self.add('Dazzling Ramparts','other',owner)

    def stats(self,ref):
        card=self.cards[ref]
        amount=card['boost']-self.minus.get(ref,0)
        record=self.oracle[card['name']]
        return tuple(int(record[prop])+amount for prop in ('power','toughness'))

    def observe(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def zones_and_life(self):
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=self.life[seat])
            self.emit('assert_poison',player=seat,value=self.poison[seat])
            for zone in ('battlefield','hand','graveyard'):
                cards=[e for e in self.cards.values() if e['player']==seat and e['zone']==zone]
                self.emit('assert_zone',player=seat,zone=zone,value=len(cards))
                for name in sorted({card['name'] for card in cards}):
                    self.emit('assert_zone_multiplicity',player=seat,zone=zone,index=self.rows[name]['csvid'],
                              value=sum(card['name']==name for card in cards))

    def facts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_unused_choices',value=0)
        self.observe_mana()
        self.zones_and_life()
        for ref,card in self.cards.items():
            if card['zone']!='battlefield':
                continue
            power,toughness=self.stats(ref)
            for prop,value in (('in_play',1),('power',power),('toughness',toughness),
                               ('damage',card['damage']),('minus1_counters',self.minus.get(ref,0)),('tapped',card['tapped'])):
                self.observe(ref,prop,value)
            if ref=='source' and self.shape['artifact']:
                self.emit('assert_matching_permanents',player=card['player'],index=self.rows[card['name']]['csvid'],
                          property='artifact',value=1)

    def play_source(self,denied=False):
        funding=list(self.shape['mana'])
        if denied:
            paid=next(i for i,mana in enumerate(funding) if mana)
            funding[paid]-=1
        self.cast('source',funding=funding,allowed=not denied)
        self.emit('require_card',ref='source',property='in_hand',value=int(denied))
        self.emit('assert_poison',player=0,value=self.poison[0])
        self.emit('assert_poison',player=1,value=self.poison[1])
        if denied:
            self.facts()
            return
        if self.mode=='countered':
            self.emit('announce_spell',ref='source')
            self.cast('counter',response=True)
            self.emit('resolve_response')
            self.cards['source']['zone']='graveyard'
            self.cards['counter']['zone']='graveyard'
        else:
            self.emit('resolve')
            self.cards['source']['zone']='battlefield'
            if self.rider=='entry_poison':
                self.poison=[n+1 for n in self.poison]
        self.emit('refresh')
        self.facts()

    def advance(self):
        if any(map(any,self.pools)):
            raise ValueError('real turn needs spent pools')
        self.emit('advance_turn')
        self.active=1-self.active
        self.emit('untap_step',player=self.active)
        self.emit('turn',player=self.active,value=0)
        for permanent in self.cards.values():
            permanent.update(damage=0,boost=0)
            if permanent['zone']=='battlefield' and permanent['player']==self.active:
                permanent['tapped']=False

    def infect(self,ref):
        return 'Infect' in self.oracle[self.cards[ref]['name']]['keywords']

    def hit(self,source,recipient,amount):
        if amount<=0:
            return
        if self.rider=='received_poison' and recipient=='source':
            self.poison[self.cards[source]['player']]+=1
        if type(recipient) is int:
            if self.infect(source):
                self.poison[recipient]+=amount
            else:
                self.life[recipient]-=amount
        else:
            if self.infect(source):
                self.minus[recipient]=self.minus.get(recipient,0)+amount
            else:
                self.cards[recipient]['damage']+=amount

    def settle(self,refs):
        for ref in refs:
            if self.stats(ref)[1]<=self.cards[ref]['damage']:
                self.cards[ref]['zone']='graveyard'

    def legal_block(self,attacker,blocker):
        return blocker and not (attacker=='source' and self.rider=='unblockable')

    def choose_bypass(self,attacker,blocked):
        return False

    def before_damage(self,attacker,blocker,blocked,fog):
        pass

    def after_damage(self,attacker,blocked,fog):
        pass

    def combat(self,attacker,blocker=None,*,fog=False,fresh=False):
        target=1-self.active
        allowed=not fresh and (attacker!='source' or self.rider!='poison_gate' or self.poison[target]>0)
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=int(allowed))
        self.observe(attacker,'attacking',allowed)
        self.emit('finish_attackers')
        if not allowed:
            self.facts()
            return
        self.cards[attacker]['tapped']=True
        if attacker=='source' and self.rider=='poison_pump' and self.poison[target]>0:
            self.cards[attacker]['boost']+=1
        self.emit('refresh')
        self.facts()
        self.emit('begin_blockers')
        blocked=self.legal_block(attacker,blocker)
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(bool(blocked)))
        bypass=self.choose_bypass(attacker,blocked)
        self.emit('advance_combat')
        if blocked and attacker=='source' and self.rider=='blocked_pump':
            self.cards[attacker]['boost']+=2
        self.observe(attacker,'blocked',bool(blocked))
        self.emit('refresh')
        self.facts()
        self.before_damage(attacker,blocker,blocked,fog)
        self.emit('combat_damage')
        self.emit('resolve')
        if not fog:
            outgoing=self.stats(attacker)[0]
            if blocked:
                incoming=self.stats(blocker)[0]
                self.hit(attacker,target if bypass else blocker,outgoing)
                self.hit(blocker,attacker,incoming)
                self.settle((attacker,blocker))
            else:
                self.hit(attacker,target,outgoing)
        self.after_damage(attacker,blocked,fog)
        self.emit('end_combat')
        self.emit('refresh')
        self.facts()

    def poison_primer(self):
        primer=self.cards.get('primer')
        if not primer:
            return
        while self.active!=primer['player']:
            self.advance()
        self.combat('primer')
        if self.active==self.owner:
            self.advance()
        self.advance()

    def spell(self,ref,target=None):
        self.cast(ref,target=target)
        self.emit('resolve')
        self.cards[ref]['zone']='graveyard'

    def shocked(self):
        for ref in ('enemy_spell','friendly_spell'):
            if ref not in self.cards:
                continue
            target='other' if self.mode=='other_creature' else 'source'
            self.spell(ref,target)
            self.hit(ref,target,2)
            self.settle((target,))
            self.emit('refresh')
            self.facts()

    def run_rider(self):
        mode=self.mode
        self.play_source(denied=mode=='short')
        if mode in ('short','countered'):
            return
        if mode=='recast':
            self.spell('return_spell','source')
            self.cards['source']['zone']='hand'
            self.emit('bind_hand',ref='source',player=self.owner,value=self.rows[self.cards['source']['name']]['csvid'])
            self.facts()
            self.play_source()
            return
        if mode in ('enemy_spell','friendly_spell','mixed_sources','other_creature'):
            self.shocked()
            return
        if mode=='fresh':
            self.combat('source',fresh=True)
            return
        self.advance()
        if mode=='defending' or (mode=='fogged' and self.rider=='received_poison'):
            if mode=='fogged':
                self.spell('fog')
            self.combat('attacker','source',fog=mode=='fogged')
            return
        if not self.cards.get('primer') or self.cards['primer']['player']==self.owner:
            self.advance()
        self.poison_primer()
        if mode=='fogged':
            self.spell('fog')
        self.combat('source','blocker' if 'blocker' in self.cards else None,fog=mode=='fogged')
        if mode in ('cleanup','poisoned_block'):
            self.emit('cleanup')
            self.cards['source']['boost']=0
            self.emit('refresh')
            self.facts()


class Followers(InfectBoard):
    def __init__(self,*args,**kwargs):
        self.insect_count=0
        super().__init__(*args,**kwargs)

    def build(self):
        super().build()
        mode,rider,owner,foe=self.mode,self.rider,self.owner,self.foe
        if mode in ('bypass','normal'):
            self.add('Dazzling Ramparts','blocker',foe)
        if rider=='rot_draw' and mode.startswith('wolf_'):
            self.add('Dazzling Ramparts','blocker',foe)
            self.add('Forest','draw_card',owner,'library')
            if mode=='wolf_unrelated':
                self.add('Grizzly Bears','other_creature',foe)
        if rider=='infect_lord' and mode!='short':
            self.add('Glistener Elf','friendly',owner)
            self.add('Glistener Elf','foreign',foe)
            self.add('Grizzly Bears','ordinary',owner)
            if mode in ('cast_enemy_poison','cast_self_poison','infect_countered','noninfect'):
                self.add('Grizzly Bears' if mode=='noninfect' else 'Glistener Elf','qualifier',owner,'hand')
            if mode=='infect_countered':
                self.add('Counterspell','counter',foe,'hand')
        if rider=='specter':
            if mode in ('discard_two','short_hand'):
                self.add('Glistener Elf','primer',owner)
            if mode in ('discard_two','short_hand','decline'):
                copies=1 if mode=='short_hand' else 3
                for index in range(copies):
                    self.add('Island',f'discard_{index}',foe,'hand')
            if mode=='flying_blocked':
                self.add('Wind Drake','blocker',foe)
        if rider=='swarm' and mode in ('swarm_scale','swarm_self_only','swarm_removed','replay'):
            self.add('Glistener Elf','primer',foe if mode=='swarm_self_only' else owner)
            if mode=='replay':
                self.add('Giant Growth','primer_growth',foe,'hand')

    def stats(self,ref):
        if ref.startswith('insect_'):
            amount=self.cards[ref]['boost']-self.minus.get(ref,0)
            return 1+amount,1+amount
        power,toughness=super().stats(ref)
        if (self.rider=='infect_lord' and ref!='source' and self.cards['source']['zone']=='battlefield'
                and self.cards[ref]['player']==self.owner and self.infect(ref)):
            return power+1,toughness+1
        return power,toughness

    def legal_block(self,attacker,blocker):
        if blocker and attacker=='source' and self.rider=='specter':
            return 'Flying' in self.oracle[self.cards[blocker]['name']]['keywords']
        return super().legal_block(attacker,blocker)

    def choose_bypass(self,attacker,blocked):
        if self.rider=='bypass' and attacker=='source' and blocked:
            bypass=self.mode in ('bypass','replay')
            self.emit('present_ai_dialogs',value=1)
            self.emit('choose_dialog',player=self.owner,value=int(bypass))
            return bypass
        return False

    def before_damage(self,attacker,blocker,blocked,fog):
        if self.rider=='bypass' and attacker=='source' and blocked:
            self.emit('present_ai_dialogs',value=0)
        if self.rider!='specter' or attacker!='source' or blocked or fog:
            return
        accept=self.mode in ('discard_two','short_hand')
        if self.owner==0:
            self.emit('choose_trigger_source',player=0,ref='source')
        else:
            self.emit('present_ai_dialogs',value=1)
        self.emit('choose_dialog',player=self.owner,value=int(not accept))
        if accept and self.foe==0:
            self.emit('discard_policy',value=1)

    def after_damage(self,attacker,blocked,fog):
        if self.rider!='specter' or attacker!='source' or blocked or fog:
            return
        if self.owner==1:
            self.emit('present_ai_dialogs',value=0)
        if self.mode not in ('discard_two','short_hand'):
            return
        self.cards['source']['zone']='graveyard'
        hand=[c for c in self.cards.values() if c['player']==self.foe and c['zone']=='hand']
        for card in hand[:self.poison[self.foe]]:
            card['zone']='graveyard'
        if self.foe==0:
            self.emit('assert_discard_choices',value=min(len(hand),self.poison[self.foe]))
            self.emit('discard_policy',value=0)

    def infect(self,ref):
        # Oracle supplies the Swarmlord-created token rules. Its physical
        # registry template is also shared by Insects from other effects.
        return ref.startswith('insect_') or super().infect(ref)

    def facts(self):
        super().facts()
        if self.rider=='swarm':
            csvid=self.rows['Insect']['csvid']
            self.emit('assert_zone_multiplicity',player=self.owner,zone='battlefield',index=csvid,value=self.insect_count)
            self.emit('assert_zone_multiplicity',player=self.foe,zone='battlefield',index=csvid,value=0)
            if self.insect_count:
                traits=(('token',1),('artifact',0),('color',8),('subtype_Insect',1))
                if self.mode!='replay':
                    traits+=(('subtype_Phyrexian',1),)
                for attr,value in traits:
                    self.emit('assert_matching_permanents',player=self.owner,index=csvid,property=attr,value=value)
        if self.rider=='rot_draw' and 'draw_card' in self.cards:
            remains=int(self.cards['draw_card']['zone']=='library')
            self.emit('assert_zone',player=self.owner,zone='library',value=remains)
            if remains:
                self.emit('assert_zone_card',player=self.owner,zone='library',index=0,value=self.rows['Forest']['csvid'])

    def real_upkeep(self):
        self.emit('upkeep_step',player=self.active)
        if self.active!=self.owner or self.cards['source']['zone']!='battlefield':
            return
        if self.rider=='upkeep_self':
            self.poison[self.owner]+=1
        else:
            quantity=self.poison[self.foe]
            for i in range(self.insect_count,self.insect_count+quantity):
                ref=f'insect_{i}'
                self.cards[ref]=dict(name='Insect',player=self.owner,zone='battlefield',damage=0,boost=0,tapped=False)
                self.emit('bind_permanent_copy',player=self.owner,index=i,ref=ref,value=self.rows['Insect']['csvid'])
            self.insect_count+=quantity
            if quantity and self.mode=='replay':
                self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Insect']['csvid'],
                          property='subtype_Phyrexian',value=1)

    def advance(self):
        if self.rider not in ('upkeep_self','swarm'):
            super().advance()
            return
        if any(map(any,self.pools)):
            raise ValueError('real upkeep requires empty pools')
        self.emit('advance_turn')
        self.active=1-self.active
        self.emit('untap_step',player=self.active)
        for permanent in self.cards.values():
            permanent.update(damage=0,boost=0)
            if permanent['zone']=='battlefield' and permanent['player']==self.active:
                permanent['tapped']=False
        self.facts()
        self.real_upkeep()
        self.emit('turn',player=self.active,value=0)
        self.emit('refresh')
        self.facts()

    def regen(self):
        fee=list(self.shape['regen'])
        saves=self.mode=='regen_paid'
        if self.mode=='regen_short':
            fee[next(i for i,amount in enumerate(fee) if amount)]-=1
        self.fund(self.owner,fee)
        if self.mode=='regen_proactive':
            self.emit('can_activate',ref='source',value=1)
            self.facts()
            return
        self.emit('choose_regeneration',ref='source')
        self.emit('destroy',ref='source')
        self.emit('assert_unused_choices',value=int(not saves))
        if saves:
            self.cards['source']['tapped']=True
            self.pools[self.owner]=[0]*6
        else:
            self.cards['source']['zone']='graveyard'
        self.facts()

    def wolf(self):
        self.advance()
        self.advance()
        self.combat('source','blocker')
        if self.mode=='wolf_next_turn':
            self.advance()
        victim='other_creature' if self.mode=='wolf_unrelated' else 'blocker'
        may_draw=self.mode in ('wolf_draw','wolf_decline')
        if may_draw:
            self.emit('present_ai_dialogs',value=1)
            self.emit('choose_dialog',player=self.owner,value=int(self.mode=='wolf_decline'))
        self.emit('destroy',ref=victim)
        if may_draw:
            self.emit('present_ai_dialogs',value=0)
        self.cards[victim]['zone']='graveyard'
        if self.mode=='wolf_draw':
            self.cards['draw_card']['zone']='hand'
        self.facts()

    def hand_trigger(self):
        from recipes import cost_from_oracle

        target=self.owner if self.mode=='cast_self_poison' else self.foe
        infect_spell=self.mode!='noninfect'
        fee=cost_from_oracle(self.oracle[self.cards['qualifier']['name']])
        self.fund(self.owner,fee)
        self.emit('cast',ref='qualifier',value=1)
        self.emit('require_card',ref='qualifier',property='in_hand',value=0)
        self.emit('assert_stack',value=1)
        self.pools[self.owner]=[0]*6
        if infect_spell:
            self.emit('choose_player',player=self.owner,value=target)
        self.emit('announce_spell',ref='qualifier')
        self.emit('assert_stack',value=1)
        self.emit('assert_unused_choices',value=0)
        self.observe_mana()
        if infect_spell:
            self.poison[target]+=1
        for who in (0,1):
            self.emit('assert_poison',player=who,value=self.poison[who])
        if self.mode=='infect_countered':
            self.cast('counter',response=True)
            self.emit('resolve_response')
            self.cards['counter']['zone']='graveyard'
            self.cards['qualifier']['zone']='graveyard'
        else:
            self.emit('resolve')
            self.cards['qualifier']['zone']='battlefield'
        self.emit('refresh')
        self.facts()

    def swarm_turns(self):
        self.advance()
        if self.mode=='swarm_self_only':
            self.combat('primer')
            self.advance()
            return
        self.advance()
        self.combat('primer')
        if self.mode=='swarm_removed':
            self.emit('destroy',ref='source')
            self.cards['source']['zone']='graveyard'
            self.facts()
            self.advance()
            self.advance()
            return
        self.advance()
        self.advance()
        if self.mode=='replay':
            # Keep the scale variant's separate Swarmlord untap receipt.
            self.spell('primer_growth','primer')
            self.cards['primer']['boost']+=3
            self.combat('primer')
        else:
            self.combat('source')
        self.advance()
        self.advance()
        self.combat('insect_0')

    def run_rider(self):
        mode=self.mode
        custom=mode in FOLLOW_CASES[self.rider] or (self.rider=='swarm' and mode=='replay')
        if not custom:
            super().run_rider()
            return
        self.play_source()
        if self.rider=='regenerate':
            self.regen()
        elif self.rider=='rot_draw':
            self.wolf()
        elif self.rider=='upkeep_self':
            self.advance()
            self.advance()
            if mode=='upkeep_removed':
                self.emit('destroy',ref='source')
                self.cards['source']['zone']='graveyard'
                self.facts()
            self.advance()
            self.advance()
        elif self.rider=='swarm':
            self.swarm_turns()
        elif self.rider=='infect_lord':
            if mode=='lord_scope':
                self.emit('destroy',ref='source')
                self.cards['source']['zone']='graveyard'
                self.emit('refresh')
                self.facts()
            else:
                self.hand_trigger()
        else:
            self.advance()
            self.advance()
            self.poison_primer()
            self.combat('source','blocker' if 'blocker' in self.cards else None)



def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('exact infect source and scenarios required')
    validate_witnesses(oracle,rows)
    gaps=['Two-player combat and damage; no multiple-blocker damage allocation or poison-loss threshold.']
    cases=[]
    for variant in selected:
        mode,seat=variant.rsplit('_',1)
        world=Followers if shape['rider'] in FOLLOW_CASES else InfectBoard
        g=world(name,shape,mode,int(seat),rows,oracle)
        g.emit('turn',player=g.owner,value=0)
        g.fund(0,[0]*6)
        g.fund(1,[0]*6)
        cut=len(g.steps)
        g.run_rider()
        if mode=='replay':
            tail=g.steps[cut:]
            g.steps=g.steps[:cut]+[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),step('restore'),
                                  *copy.deepcopy(tail),step('assert_recorded_state')]
        cases.append(dict(name=f'InfectCombatRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=2119,setup=g.setup,steps=g.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
