"""Actual Spirit and Arcane spell announcements and complete printed rewards."""
from collections import Counter
import json
from pathlib import Path

from cast_trigger_case_support import CastTriggerBoard
from library_search_recipes import wave6_exact
from recipes import cost_from_oracle


FAMILY = 'cast_spirit_or_arcane_riders'
PROFILE = Path(__file__).parent / 'data/cast_spirit_or_arcane_riders_wave8_profiles.json'
TRIGGER = 'Whenever you cast a Spirit or Arcane spell, '
EFFECTS = {
    ('Flying', TRIGGER+"destroy all permanents with that spell's mana value."): (
        'sweep_mv', ('self_mv','flight_ground','flight_reach'), ()),
    ('Flying', TRIGGER+"you may have target player mill X cards, where X is that spell's mana value."): (
        'mill', ('self_mill','short_library','flight_ground','flight_reach'), ('decline',)),
    (TRIGGER+'this creature deals 2 damage to each creature without flying.',): (
        'ground_sweep', (), ()),
    (TRIGGER+'each other creature you control gets +1/+1 and gains trample until end of turn.',): (
        'team', ('trample','expires'), ()),
    (TRIGGER+'you may search your library for an Aura card with enchant creature, reveal it, put it into your hand, then shuffle.',): (
        'aura_search', (), ('decline','fail_to_find')),
}
FOLLOWERS = {
    ('When this creature enters, draw a card.', TRIGGER+"you may return this creature to its owner's hand."):
        ('bounce_draw', ('recast',), ('decline',)),
    ('Flying', TRIGGER+"you may exile Hikari. If you do, return it to the battlefield under its owner's control at the beginning of the next end step."):
        ('blink', ('flight_ground','flight_reach'), ('decline',)),
    ('Flying', TRIGGER+"target player reveals their hand and discards all cards with that spell's mana value."):
        ('same_mv_discard', ('self_hand','flight_ground','flight_reach'), ()),
    (TRIGGER+'this artifact becomes a 4/4 Spirit artifact creature until end of turn.',):
        ('animation', ('attack_animated','expires'), ()),
    (TRIGGER+"target creature can't block this turn.",):
        ('cannot_block', ('other_blocker','expires'), ()),
    ('Flying', TRIGGER+"target creature gains fear until end of turn. (It can't be blocked except by artifact creatures and/or black creatures.)"):
        ('fear', ('black_blocks','artifact_blocks','expires','flight_ground','flight_reach'), ()),
    ('When this creature enters, target player discards a card.', TRIGGER+"you may return this creature to its owner's hand."):
        ('bounce_discard', ('recast',), ('decline',)),
    (TRIGGER+'target opponent exiles a card from their hand.',):
        ('hand_exile', ('empty_hand','protected_opponent'), ()),
    ('Flying', TRIGGER+"you may gain control of target creature with that spell's mana value until end of turn."):
        ('steal', ('wrong_mv','flight_ground','flight_reach'), ('decline',)),
}
FOLLOW_EFFECTS = {entry[0] for entry in FOLLOWERS.values()}
FOLLOW_OPTIONAL = {'bounce_draw','bounce_discard','blink','steal'}

COMMON = ('own_spirit','own_arcane','own_other_creature','own_other_instant','enemy_spirit','enemy_arcane')
OPTIONAL = {'mill','aura_search'}
ARCANE = 'Reach Through Mists'
ARCANE_TUTOR = 'Lava Spike'
SPIRIT = 'Kami of Ancient Law'
OTHER_CREATURE = 'Grizzly Bears'
OTHER_INSTANT = 'Think Twice'
BIG_SPIRIT = 'Flickering Spirit'
ONE = 'Merfolk of the Pearl Trident'
FLYER = 'Suntail Hawk'
REACH = 'Wall of Vines'
BLOCKER = "Mons's Goblin Raiders"
AURA = 'Pacifism'
FAKE_AURA = 'Spreading Seas'
NON_AURA = 'Glorious Anthem'
DECK = ('Forest','Island','Plains','Swamp')
DUMMIES = {OTHER_CREATURE,ONE,FLYER,REACH,BLOCKER,'Hill Giant','Bronze Sable'}
WITNESSES = {*DECK,*DUMMIES,ARCANE,SPIRIT,OTHER_INSTANT,BIG_SPIRIT,AURA,FAKE_AURA,NON_AURA,ARCANE_TUTOR,
             'Scathe Zombies','Wall of Tanglecord','Leyline of Sanctity'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    if not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal Spirit or Arcane reward Oracle required')
    lines=tuple(record['oracle_text'].splitlines())
    patterns=EFFECTS|FOLLOWERS
    if lines not in patterns:
        raise ValueError('unreviewed complete Spirit or Arcane reward')
    effect, extra, human=patterns[lines]
    flying='Flying' in lines
    if set(record['keywords'])!=({'Flying'} if flying else set()) | ({'Mill'} if effect=='mill' else set()):
        raise ValueError('printed keyword set changed')
    stats=(record.get('power'),record.get('toughness'))
    if effect=='animation':
        if record['type_line']!='Artifact' or any(stat is not None for stat in stats):
            raise ValueError('exact noncreature artifact required')
        power,toughness=None,None
    else:
        if any(not isinstance(n,str) or not n.isdecimal() for n in stats) or 'Creature' not in record['type_line']:
            raise ValueError('fixed printed creature characteristics expected')
        power,toughness=map(int,stats)
    return dict(effect=effect,price=cost_from_oracle(record),flying=flying,
                power=power,toughness=toughness,extra=list(extra),human=list(human))


def variants(shape):
    return ([f'{case}_{owner}' for case in (*COMMON,*shape['extra']) for owner in (0,1)]
            + [f'{case}_0' for case in shape['human']])


def spec(declaration, record):
    pin=profiles()['sources'].get(record.get('name') if record else None)
    if not pin or not wave6_exact(pin['oracle'],record):
        raise ValueError('exact pinned complete Oracle record required')
    shape=classify(record)
    wanted=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text']
            or not wave6_exact(declaration.get('params'),shape) or not isinstance(wanted,list)
            or not wanted or len(set(wanted))!=len(wanted) or set(wanted)-set(variants(shape))):
        raise ValueError('exact Spirit/Arcane recipe and distinct reviewed variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle, rows):
    data=profiles()
    if set(data)!={'sources','witnesses'} or set(data['witnesses'])!=WITNESSES:
        raise ValueError('whole pinned source and independent spell witnesses required')
    for group,pins in data.items():
        for name,pin in pins.items():
            row,record=rows[name],oracle[name]
            if (pin['registry']!=row or not wave6_exact(pin['oracle'],record) or row['kind']!='card'
                    or row['handler'] in (None,'0','NULL') or row['handler']=='card_dummy' and name not in DUMMIES):
                raise ValueError('unreviewed Oracle or native witness '+name)
            if group=='sources':
                classify(record)


class Board(CastTriggerBoard):
    def build(self):
        self.effect=self.shape['effect']
        self.life=[20,20]
        self.active=self.owner
        self.buff=set()
        self.shuffles=[0,0]
        if self.mode not in ('flight_ground','flight_reach'):
            caster=self.foe if self.mode.startswith('enemy_') else self.owner
            witness={'own_spirit':SPIRIT,'enemy_spirit':SPIRIT,'own_other_creature':OTHER_CREATURE,
                     'own_other_instant':OTHER_INSTANT,'self_mv':BIG_SPIRIT,'short_library':SPIRIT,
                     'decline':SPIRIT,'fail_to_find':SPIRIT}.get(self.mode,ARCANE)
            if self.effect=='aura_search' and self.mode=='own_arcane':
                witness=ARCANE_TUTOR
            self.add(witness,'spell',caster,'hand')
        for seat in (0,1):
            sequence=DECK
            if self.effect=='aura_search':
                sequence=(FAKE_AURA,NON_AURA,AURA,'Forest','Island')
            if seat==self.foe and self.mode=='short_library':
                sequence=DECK[:1]
            for pos,name in enumerate(sequence):
                self.add(name,f'library_{seat}_{pos}',seat,'library')
        if self.mode in ('flight_ground','flight_reach'):
            self.add(REACH if self.mode=='flight_reach' else OTHER_CREATURE,'blocker',self.foe)
        if self.effect=='sweep_mv' and 'spell' in self.cards:
            for side in (0,1):
                for label,name in [('one',ONE),('two',OTHER_CREATURE),('four','Hill Giant'),('zero','Forest')]:
                    self.add(name,f'{label}_{side}',side,tapped=label=='zero')
        if self.effect=='ground_sweep':
            for side in (0,1):
                for label,name in [('weak',OTHER_CREATURE),('strong','Hill Giant'),('flying',FLYER)]:
                    self.add(name,f'{label}_{side}',side)
        if self.effect=='team':
            self.add(OTHER_CREATURE,'ally',self.owner)
            self.add('Bronze Sable','artifact',self.owner)
            self.add(BLOCKER,'blocker',self.foe)
            self.add(OTHER_CREATURE,'foreign',self.foe)

    def creature(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def stats(self,ref):
        oracle=self.oracle[self.cards[ref]['name']]
        return int(oracle['power'])+(ref in self.buff),int(oracle['toughness'])+(ref in self.buff)

    def deck(self,side):
        return [card for card in self.cards.values() if card['player']==side and card['zone']=='library']

    def receipts(self):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.observe_mana()
        for side in (0,1):
            self.emit('assert_life',player=side,value=self.life[side])
            if self.effect=='aura_search':
                self.emit('assert_shuffle_count',player=side,value=self.shuffles[side])
            for zone in ('hand','battlefield','graveyard','library','exile'):
                ids=[self.rows[item['name']]['csvid'] for item in self.cards.values() if item['player']==side and item['zone']==zone]
                self.emit('assert_zone',player=side,zone=zone,value=len(ids))
                if zone=='library' and not self.shuffles[side]:
                    for pos,csvid in enumerate(ids):
                        self.emit('assert_zone_card',player=side,zone=zone,index=pos,value=csvid)
                else:
                    for csvid,n in sorted(Counter(ids).items()):
                        self.emit('assert_zone_multiplicity',player=side,zone=zone,index=csvid,value=n)
        for ref,item in self.cards.items():
            if item['zone']=='battlefield' and 'Creature' in self.oracle[item['name']]['type_line']:
                power,toughness=self.stats(ref)
                for prop,value in [('power',power),('toughness',toughness),('damage',item['damage']),('tapped',item['tapped'])]:
                    self.creature(ref,prop,value)

    def next_turn(self):
        self.emit('advance_turn')
        self.active=1-self.active
        self.buff.clear()
        for item in self.cards.values():
            item['damage']=0
            if item['zone']=='battlefield' and item['player']==self.active:
                item['tapped']=False
        self.emit('untap_step',player=self.active)
        self.emit('turn',player=self.active,value=0)

    def mill_target(self):
        return self.owner if self.mode=='self_mill' else self.foe

    def selected(self,ref):
        typ=self.oracle[self.cards[ref]['name']]['type_line'].partition(' — ')[2].split()
        return self.cards[ref]['player']==self.owner and bool({'Spirit','Arcane'}.intersection(typ))

    def choices(self,_):
        if not self.selected('spell') or self.mode=='decline':
            return
        if self.effect in OPTIONAL and self.owner==0:
            self.emit('choose_trigger_source',player=self.owner,ref='source')
        if self.effect=='mill':
            self.emit('choose_player',player=self.owner,value=self.mill_target())
        if self.effect=='aura_search':
            if self.owner==0:
                self.emit('observe_search_filters',value=1)
            self.emit('choose_optional_zone' if self.mode=='fail_to_find' else 'choose_zone_card',
                      player=self.owner,value=self.rows[AURA]['csvid'])

    def reward(self):
        amount=int(self.oracle[self.cards['spell']['name']]['cmc'])
        if self.effect=='sweep_mv':
            for item in self.cards.values():
                if item['zone']=='battlefield' and int(self.oracle[item['name']]['cmc'])==amount:
                    item['zone']='graveyard'
        elif self.effect=='mill':
            for item in self.deck(self.mill_target())[:amount]:
                item['zone']='graveyard'
        elif self.effect=='ground_sweep':
            for ref,item in self.cards.items():
                stats=self.oracle[item['name']]
                if item['zone']=='battlefield' and 'Creature' in stats['type_line'] and 'Flying' not in stats['keywords']:
                    item['damage']+=2
                    if item['damage']>=self.stats(ref)[1]:
                        item['zone']='graveyard'
        elif self.effect=='team':
            self.buff={ref for ref,item in self.cards.items() if ref!='source' and item['zone']=='battlefield'
                       and item['player']==self.owner and 'Creature' in self.oracle[item['name']]['type_line']}
        elif self.effect=='aura_search':
            self.shuffles[self.owner]+=1
            if self.mode!='fail_to_find':
                target=next(item for item in self.deck(self.owner) if item['name']==AURA)
                target['zone']='hand'

    def trigger(self):
        item=self.cards['spell']
        record=self.oracle[item['name']]
        if item['player']!=self.active:
            self.next_turn()
        if item['name']==ARCANE_TUTOR:
            self.emit('choose_player',player=item['player'],value=1-item['player'])
        self.cast_trigger_spell('spell',choices=self.choices,resolve=False)
        did_reward=self.selected('spell') and self.mode!='decline'
        if did_reward:
            self.reward()
        self.emit('resolve')
        if 'Instant' in record['type_line']:
            item['zone']='graveyard'
            self.draw(item['player'])
        elif item['name']==ARCANE_TUTOR:
            item['zone']='graveyard'
            self.life[1-item['player']]-=3
        else:
            item['zone']='battlefield'
        if self.effect=='aura_search' and did_reward and self.owner==0:
            self.emit('assert_offered_count',value=5)
            self.emit('assert_offered_selection_required',value=0)
            for name in (AURA,FAKE_AURA,NON_AURA):
                self.emit('assert_offered_legal_card',index=self.rows[name]['csvid'],value=int(name==AURA))
        self.receipts()

    def combat(self,ref,can_block):
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=1)
        self.cards[ref]['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other=ref,value=int(can_block))
        self.emit('advance_combat')
        self.creature(ref,'blocked',can_block)
        self.emit('combat_damage')
        power,toughness=self.stats(ref)
        if can_block:
            blocker=self.cards['blocker']
            bp,bt=self.stats('blocker')
            excess=max(power-bt,0) if ref in self.buff else 0
            self.life[self.foe]-=excess
            blocker['damage']=power-excess
            self.cards[ref]['damage']=bp
            if blocker['damage']>=bt:
                blocker['zone']='graveyard'
            if bp>=toughness:
                self.cards[ref]['zone']='graveyard'
        else:
            self.life[self.foe]-=power
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def run(self):
        self.play_source()
        self.receipts()
        if self.mode in ('flight_ground','flight_reach'):
            self.next_turn()
            self.next_turn()
            self.combat('source',self.mode=='flight_reach')
            return
        self.trigger()
        if self.mode=='expires':
            self.next_turn()
            self.next_turn()
            self.receipts()
        if self.mode in ('trample','expires'):
            self.combat('ally',True)


class FollowerBoard(Board):
    def build(self):
        super().build()
        self.animated=False
        self.transfer_pending=False
        mode=self.mode
        if mode in ('flight_ground','flight_reach'):
            return
        if mode in ('recast','self_hand','wrong_mv'):
            self.cards['spell']['name']=SPIRIT
            next(card for card in self.setup if card['ref']=='spell')['name']=SPIRIT
        if self.effect=='bounce_discard':
            self.entry_target=self.owner if mode.startswith('enemy_') else self.foe
            for i in range(2):
                self.add('Forest',f'entry_discard_{i}',self.entry_target,'hand')
        if self.effect=='same_mv_discard':
            side=self.owner if mode=='self_hand' else self.foe
            for i,name in enumerate((ONE,FLYER,OTHER_CREATURE,'Bronze Sable','Island','Hill Giant')):
                self.add(name,f'discard_{i}',side,'hand')
        if self.effect in ('cannot_block','fear'):
            self.add(OTHER_CREATURE,'ally',self.owner)
            blocker={'black_blocks':'Scathe Zombies','artifact_blocks':'Wall of Tanglecord'}.get(mode,BLOCKER)
            self.add(blocker,'blocker',self.foe)
            if mode=='other_blocker':
                self.add(BLOCKER,'other_blocker',self.foe)
        if self.effect=='hand_exile' and mode!='empty_hand':
            self.add('Forest','exile_choice',self.foe,'hand')
            if mode=='protected_opponent':
                self.add('Leyline of Sanctity','leyline',self.foe)
        if self.effect=='steal':
            spell=self.oracle[self.cards['spell']['name']]
            name=OTHER_CREATURE if spell['cmc']==2 and mode!='wrong_mv' else ONE
            self.add(name,'recipient',self.foe)

    def etb_choices(self,_):
        if self.effect=='bounce_discard':
            self.emit('choose_player',player=self.owner,value=self.entry_target)

    def move_entry_effect(self):
        if self.effect=='bounce_draw':
            self.draw(self.owner)
        elif self.effect=='bounce_discard':
            card=next(card for ref,card in self.cards.items() if ref.startswith('entry_discard_') and card['zone']=='hand')
            card['zone']='graveyard'

    def choices(self,_):
        if not self.selected('spell') or self.mode=='decline':
            return
        if self.effect in FOLLOW_OPTIONAL and self.owner==0:
            self.emit('choose_trigger_source',player=self.owner,ref='source')
        if self.effect=='same_mv_discard':
            self.emit('choose_player',player=self.owner,value=self.owner if self.mode=='self_hand' else self.foe)
        elif self.effect in ('cannot_block','fear'):
            self.emit('choose_target',player=self.owner,ref='blocker' if self.effect=='cannot_block' else 'ally')
        elif self.effect=='hand_exile' and self.mode not in ('empty_hand','protected_opponent'):
            self.emit('choose_zone_card',player=self.foe,value=self.rows['Forest']['csvid'])
        elif self.effect=='steal' and self.mode!='wrong_mv':
            self.emit('choose_target',player=self.owner,ref='recipient')

    def reward(self):
        if self.effect in ('bounce_draw','bounce_discard'):
            self.cards['source']['zone']='hand'
        elif self.effect=='blink':
            self.cards['source']['zone']='exile'
        elif self.effect=='same_mv_discard':
            side=self.owner if self.mode=='self_hand' else self.foe
            match=self.oracle[self.cards['spell']['name']]['cmc']
            for card in self.cards.values():
                if card['zone']=='hand' and card['player']==side and self.oracle[card['name']]['cmc']==match:
                    card['zone']='graveyard'
        elif self.effect=='animation':
            self.animated=True
        elif self.effect=='hand_exile' and self.mode not in ('empty_hand','protected_opponent'):
            self.cards['exile_choice']['zone']='exile'
        elif self.effect=='steal' and self.mode!='wrong_mv':
            self.cards['recipient']['player']=self.owner
            self.transfer_pending=True

    def rebind(self,ref,*,hand=False):
        item=self.cards[ref]
        self.emit('bind_hand' if hand else 'bind_permanent',ref=ref,player=item['player'],value=self.rows[item['name']]['csvid'])

    def creature(self,ref,prop,value):
        if (self.effect=='steal' and ref=='recipient'
                and self.mode in ('own_arcane','own_spirit')):
            item=self.cards[ref]
            self.emit('assert_matching_permanents',player=item['player'],
                      index=self.rows[item['name']]['csvid'],property=prop,value=int(value))
            return
        super().creature(ref,prop,value)

    def receipts(self):
        if self.transfer_pending:
            # A legal same-MV target must be offered. Consume the typed plan
            # before reporting a missing offer so the controller and return
            # observations also execute when the game rejects this target.
            self.emit('assert_unused_choices',value=0)
            self.transfer_pending=False
        super().receipts()

    def finish_blink(self):
        if self.cards['source']['zone']!='exile':
            return
        self.emit('cleanup')
        self.emit('resolve')
        self.cards['source']['zone']='battlefield'
        self.rebind('source')
        self.receipts()

    def cleanup_steal(self):
        item=self.cards['recipient']
        if item['player']!=self.owner:
            return
        self.emit('cleanup')
        self.emit('resolve')
        item['player']=self.foe
        self.receipts()

    def animation_receipt(self):
        idx=self.rows[self.cards['source']['name']]['csvid']
        for prop,result in [('artifact',1),('subtype_Spirit',int(self.animated))]:
            self.emit('assert_matching_permanents',player=self.owner,index=idx,property=prop,value=result)
        if self.animated:
            for prop in ('power','toughness'):
                self.creature('source',prop,4)

    def animation_attack(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=int(self.animated))
        self.cards['source']['tapped']=self.animated
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        if self.animated:
            self.life[self.foe]-=4
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()

    def run(self):
        if self.effect=='bounce_discard':
            self.emit('discard_policy',value=1)
        self.play_source(before_resolve=self.etb_choices)
        self.move_entry_effect()
        self.receipts()
        animation_combat=self.mode in ('attack_animated','expires') and self.effect=='animation'
        flying_combat=self.mode in ('flight_ground','flight_reach')
        if flying_combat or animation_combat:
            self.next_turn()
            self.next_turn()
        if flying_combat:
            self.combat('source',self.mode=='flight_reach')
            return
        self.trigger()
        if self.mode=='recast':
            self.rebind('source',hand=True)
            self.play_source(before_resolve=self.etb_choices)
            self.move_entry_effect()
            self.receipts()
        if self.effect=='blink':
            self.finish_blink()
        if self.effect=='steal':
            self.cleanup_steal()
        if self.effect=='animation':
            self.animation_receipt()
            if self.mode=='expires':
                self.next_turn()
                self.next_turn()
                self.animated=False
                self.animation_receipt()
            if animation_combat:
                self.animation_attack()
        if self.effect in ('cannot_block','fear') and self.active==self.owner:
            eligible=self.selected('spell')
            if self.mode=='expires':
                self.next_turn()
                self.next_turn()
                eligible=False
            blocker='other_blocker' if self.mode=='other_blocker' else 'blocker'
            unblocked=eligible and self.mode not in ('other_blocker','black_blocks','artifact_blocks')
            self.combat_with_blocker(blocker,unblocked)

    def combat_with_blocker(self,blocker,unblocked):
        self.emit('begin_attackers')
        self.emit('attack',ref='ally',value=1)
        self.cards['ally']['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref=blocker,other='ally',value=int(not unblocked))
        self.emit('advance_combat')
        self.creature('ally','blocked',not unblocked)
        self.emit('combat_damage')
        if unblocked:
            self.life[self.foe]-=2
        else:
            bp,bt=self.stats(blocker)
            self.cards[blocker]['damage']=2
            self.cards['ally']['damage']=bp
            if bt<=2:
                self.cards[blocker]['zone']='graveyard'
            if bp>=2:
                self.cards['ally']['zone']='graveyard'
        self.emit('resolve')
        self.emit('end_combat')
        self.receipts()


def build(name,csvid,declaration,shape,variant,rows,oracle):
    mode,seat=variant.rsplit('_',1)
    board=FollowerBoard if shape['effect'] in FOLLOW_EFFECTS else Board
    world=board(name,shape,mode,int(seat),rows,oracle)
    world.run()
    return dict(name=f'CastSpiritOrArcaneRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                seed=2226,setup=world.setup,steps=world.steps,
                _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                             oracle_text=declaration['oracle_text'],
                             untested_components=['Reveal presentation, multiplayer, and splice.']))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('distinct exact cases for the pinned native Spirit or Arcane source required')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
