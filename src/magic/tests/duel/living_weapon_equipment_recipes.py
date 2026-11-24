"""Real Living Weapon entry, token lifecycles, Equip, and combat keywords."""

from collections import ChainMap
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_turn_up_team_recipes import TeamScene


FAMILY = 'living_weapon_equipment'
PROFILE = Path(__file__).parent / 'data/living_weapon_equipment_wave8_profiles.json'
GERM = 'Phyrexian Germ'
GERM_ID = 18741
LIVING_WEAPON = 'Living weapon (When this Equipment enters, create a 0/0 black Phyrexian Germ creature token, then attach this to it.)'
BONUS = re.compile(r'Equipped creature gets \+([0-9]+)/\+([1-9][0-9]*)(?: and has (flying|haste|reach))?\.')
EQUIP = re.compile(r'Equip \{([1-9][0-9]*)\}')
MODES = ('enter', 'cast_short', 'countered', 'move', 'equip_short', 'cancel', 'offturn',
         'spell_pending', 'foreign_target', 'equip_target_leaves', 'source_lost_pending',
         'remove_source', 'bounce_source', 'destroy_germ', 'bounce_germ', 'host_lost',
         'keep_germ_until_cleanup', 'fresh_attack', 'ready_ground_block', 'opponent_flyer',
         'control_transfer', 'replay_entry', 'replay_move')
EXTRA = {
    'haste': ('fresh_host_move', 'fresh_host_lost'),
    'flying': ('transferred_keyword', 'keyword_lost'),
    'reach': ('transferred_keyword', 'keyword_lost'),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('selected_face') or 'card_faces' in record or
            record.get('type_line')!='Artifact — Equipment' or set(record.get('keywords',()))!={'Living weapon','Equip'}):
        raise ValueError('whole original Living Weapon equipment required')
    lines = record['oracle_text'].splitlines()
    rider = rider_parameters(record, lines)
    if rider is not None:
        return rider
    mods = BONUS.fullmatch(lines[1]) if len(lines)==3 and lines[0]==LIVING_WEAPON else None
    equip = EQUIP.fullmatch(lines[2]) if mods else None
    if not equip:
        raise ValueError('complete Living Weapon, fixed bonuses, and generic Equip required')
    return dict(effect='living_weapon',cast_mana=cost_from_oracle(record),equip_generic=int(equip[1]),
                power_delta=int(mods[1]),toughness_delta=int(mods[2]),keyword=mods[3] or '')


def variants(model):
    if 'rider' in model:
        return [f'{mode}_{actor}' for actor in (0,1) for mode in (*RIDER_COMMON,*RIDER_MODES[model['rider']])]
    modes = MODES + EXTRA.get(model['keyword'], ())
    return [f'{mode}_{actor}' for actor in (0,1) for mode in modes]


def spec(declaration, record):
    model = parameters(record)
    profile_group = 'rider_sources' if 'rider' in model else 'sources'
    pin = pins()[profile_group].get(record['name'],{})
    chosen = declaration.get('variants')
    if (not exact(pin.get('oracle'),record) or declaration.get('family')!=FAMILY or
            type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model) or
            type(chosen) is not list or not chosen or any(type(v) is not str for v in chosen) or
            len(set(chosen))!=len(chosen) or set(chosen)-set(variants(model))):
        raise ValueError('independently pinned whole Equipment and distinct typed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, rows):
    roster = pins()
    if not exact(roster['native_germ'],rows.get(GERM)) or roster['native_germ']['csvid']!=GERM_ID:
        raise ValueError('native Germ identity changed')
    for group in ('sources','witnesses'):
        for name,pin in roster[group].items():
            row = rows.get(name,{})
            if (not exact(pin,dict(oracle=oracle.get(name),registry=row)) or row.get('kind')!='card' or
                    row.get('handler') in (None,'NULL','0','card_dummy') or oracle[name].get('layout')!='normal'):
                raise ValueError(f'implemented original Equipment or witness changed: {name}')


class LivingWeaponScene(TeamScene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        # The token's characteristics come from the complete reminder clause
        # validated above. Registry data pins only the native runtime identity.
        record = dict(type_line='Token Creature — Phyrexian Germ',power='0',toughness='0',colors=['B'],keywords=[])
        super().__init__(name,model,mode,owner,rows,ChainMap({GERM:record},oracle))
        self.attached = None

    def stats(self,ref):
        power,toughness = super().stats(ref)
        if self.attached==ref:
            return power+self.model['power_delta'],toughness+self.model['toughness_delta']
        return power,toughness

    def leave(self,ref,zone):
        super().leave(ref,'ceased' if ref=='germ' else zone)
        if ref in ('source',self.attached):
            self.attached = None

    def source_traits(self,full=False):
        self.prop('source','csvid',self.csvid)
        if full:
            actor = self.entities['source']['controller']
            for prop,value in (('artifact',1),('token',0),('color',0)):
                self.emit('assert_matching_permanents',player=actor,index=self.csvid,property=prop,value=value)

    def receipts(self):
        super().receipts()
        germ = self.entities.get('germ',{})
        if germ.get('zone')=='battlefield':
            for prop,value in (('artifact',0),('token',1),('color',2),('subtype_Phyrexian',1)):
                self.emit('assert_matching_permanents',player=germ['controller'],index=GERM_ID,property=prop,value=value)

    def create_germ(self):
        actor = self.entities['source']['controller']
        self.entities['germ'] = dict(name=GERM,owner=actor,controller=actor,zone='battlefield',
                                     tapped=False,damage=0,base=(0,0),bonus=(0,0))
        self.attached = 'germ'
        self.emit('assert_zone_multiplicity',player=actor,zone='battlefield',index=GERM_ID,value=1)
        self.emit('bind_permanent_copy',ref='germ',player=actor,value=GERM_ID,index=0)

    def setup_cards(self):
        mode = self.mode
        self.add(self.name,'source',self.owner,'hand')
        fresh = mode in ('fresh_host_move','fresh_host_lost')
        self.add('Goblin Tinkerer','ally',self.owner,'hand' if fresh else 'battlefield')
        sky = mode in ('opponent_flyer','transferred_keyword','keyword_lost')
        self.add('Dragon Whelp' if sky else 'Goblin Tinkerer','enemy',self.foe)
        answer = {
            'countered':'Counterspell', 'spell_pending':'Giant Growth','equip_target_leaves':'Unsummon',
            'source_lost_pending':'Disenchant','remove_source':'Disenchant','bounce_source':'Boomerang',
            'destroy_germ':'Murder','bounce_germ':'Unsummon','host_lost':'Murder',
            'fresh_host_lost':'Disenchant','keyword_lost':'Disenchant',
        }.get(mode)
        if answer:
            self.add(answer,'answer',self.foe,'hand')
        if mode=='keep_germ_until_cleanup':
            self.add('Giant Growth','growth',self.owner,'hand')
        if mode=='control_transfer':
            self.add('Bazaar Trader','trader',self.owner)

    def cast_equipment(self):
        mode = self.mode
        fee = self.model['cast_mana']
        budget = list(fee)
        allowed = mode!='cast_short'
        if not allowed:
            budget[0] -= 1
        self.fund(self.owner,budget)
        if mode=='replay_entry':
            self.replay = len(self.steps)
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.pay(self.owner,fee)
        self.entities['source']['zone'] = 'stack'
        self.source_traits()
        self.pools()
        if mode=='countered':
            self.emit('announce_spell',ref='source')
            self.spell('answer',depth=2,response=True)
            self.resolve(parent=True)
            self.leave('answer','graveyard')
            self.leave('source','graveyard')
            self.receipts()
            return False
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.create_germ()
        self.receipts()
        return True

    def spell(self,ref,target=None,depth=1,response=False):
        from recipes import cost_from_oracle

        e = self.entities[ref]
        actor = e['owner']
        fee = cost_from_oracle(self.oracle[e['name']])
        self.fund(actor,fee)
        if target is not None:
            self.emit('choose_target',player=actor,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=depth)
        self.pay(actor,fee)
        e['zone'] = 'stack'
        self.pools()

    def begin_equip(self,target,*,available=True,cancel=False,short=False,pending=False):
        actor = self.entities['source']['controller']
        price = self.model['equip_generic']
        self.fund(actor,(price-int(short),0,0,0,0,0))
        if self.mode=='replay_move':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(available))
        if available:
            if cancel:
                self.emit('choose_cancel_target',player=actor)
            else:
                self.emit('choose_target',player=actor,ref=target)
        success = available and not cancel
        self.emit('activate',ref='source',value=int(success))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(success or pending))
        if not success:
            self.pools()
            if not pending:
                self.receipts()
            return False
        self.pay(actor,(price,0,0,0,0,0))
        self.pools()
        self.prop('source','tapped',0)
        for ref in (self.attached,target):
            for prop,value in zip(('power','toughness'),self.stats(ref)):
                self.prop(ref,prop,value)
        return True

    def remove(self,target,zone):
        self.spell('answer',target)
        self.resolve()
        self.leave('answer','graveyard')
        self.leave(target,zone)
        self.reap()
        self.receipts()

    def finish_equip(self,target):
        mode = self.mode
        if mode in ('equip_target_leaves','source_lost_pending'):
            departing = target if mode=='equip_target_leaves' else 'source'
            self.spell('answer',departing,depth=2)
            self.resolve()
            self.leave('answer','graveyard')
            self.leave(departing,'hand' if departing==target else 'graveyard')
        else:
            self.resolve()
            self.attached = target
        self.reap()
        self.receipts()

    def own_attack(self,attacker,legal=True,ground=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=int(legal))
        if legal:
            self.entities[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocks = {}
        if ground:
            blocked = self.model['keyword']!='flying'
            self.emit('block',ref='enemy',other=attacker,value=int(blocked))
            if blocked:
                blocks[attacker] = 'enemy'
        self.emit('advance_combat')
        self.combat_finish((attacker,) if legal else (),blocks)

    def oppose_flyer(self, blocker):
        self.advance()
        self.emit('begin_attackers')
        self.emit('attack',ref='enemy',value=1)
        self.entities['enemy']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        legal = self.attached==blocker and self.model['keyword'] in ('flying','reach')
        self.emit('block',ref=blocker,other='enemy',value=int(legal))
        self.emit('advance_combat')
        self.combat_finish(('enemy',),{'enemy':blocker} if legal else {})

    def follow_entry(self):
        mode = self.mode
        if mode=='fresh_attack':
            self.own_attack('germ',legal=self.model['keyword']=='haste')
        elif mode=='ready_ground_block':
            self.advance()
            self.advance()
            self.own_attack('germ',ground=True)
        elif mode=='opponent_flyer':
            self.oppose_flyer('germ')
        elif mode in ('destroy_germ','bounce_germ'):
            self.remove('germ','graveyard' if mode=='destroy_germ' else 'hand')
        elif mode in ('remove_source','bounce_source'):
            self.remove('source','graveyard' if mode=='remove_source' else 'hand')
            if mode=='bounce_source':
                self.emit('bind_hand_copy',ref='source',player=self.owner,index=0,value=self.csvid)
                self.cast_equipment()

    def equip_sequence(self):
        mode = self.mode
        target = 'ally'
        if mode=='control_transfer':
            self.donate('source')
            self.advance()
            target = 'enemy'
        if mode=='offturn':
            self.advance()
        if mode=='foreign_target':
            self.emit('probe_activation_target',ref='source',other='enemy',index=-1,value=0)
        if mode=='spell_pending':
            self.spell('answer','enemy')
        if mode=='keep_germ_until_cleanup':
            self.spell('growth','germ')
            self.resolve()
            self.leave('growth','graveyard')
            self.entities['germ']['bonus'] = (3,3)
            self.receipts()
        if mode in ('fresh_host_move','fresh_host_lost'):
            self.spell('ally')
            self.resolve()
            self.entities['ally']['zone'] = 'battlefield'
            self.receipts()
        denied = mode in ('equip_short','offturn','spell_pending')
        if not self.begin_equip(target,available=not denied,cancel=mode=='cancel',
                                short=mode=='equip_short',pending=mode=='spell_pending'):
            if mode=='spell_pending':
                self.resolve()
                self.leave('answer','graveyard')
                self.entities['enemy']['bonus'] = (3,3)
                self.receipts()
            return
        self.finish_equip(target)
        if mode=='keep_germ_until_cleanup':
            self.emit('cleanup')
            self.emit('refresh')
            self.entities['germ']['bonus'] = (0,0)
            self.reap()
            self.receipts()
        elif mode=='host_lost':
            self.remove('ally','graveyard')
        elif mode in ('fresh_host_move','fresh_host_lost'):
            if mode=='fresh_host_lost':
                self.remove('source','graveyard')
            self.own_attack('ally',legal=mode=='fresh_host_move')
        elif mode in ('transferred_keyword','keyword_lost'):
            if mode=='keyword_lost':
                self.remove('source','graveyard')
            self.oppose_flyer('ally')

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for p in (0,1):
            for color in range(6):
                self.emit('set_mana',player=p,color=color,value=0)
        self.emit('refresh')
        if not self.cast_equipment():
            return
        if self.mode in ('enter','replay_entry','fresh_attack','ready_ground_block','opponent_flyer',
                         'destroy_germ','bounce_germ','remove_source','bounce_source'):
            self.follow_entry()
        else:
            self.equip_sequence()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if 'rider' in model:
        return expand_rider(name,csvid,declaration,model,selected,rows,oracle)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved Living Weapon source')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene = LivingWeaponScene(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [
                step('record_state'),step('ai_roundtrip'),step('restore')] + suffix + [step('assert_recorded_state')]
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=['Direct Germ subtype query beyond native Germ identity and the Phyrexian observer',
                                             'Direct attachment-pointer observation','Multiplayer'])
        cases.append(dict(name=f'LivingWeaponEquipmentRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=123,setup=scene.setup,steps=steps,_recipe=metadata))
    return cases


# Rider records are separate from the original fixed-bonus profiles.
RIDER_EFFECTS = {
    ('Equipped creature gets +4/+4 and has vigilance and lifelink.',
     "{3}: Return this Equipment to its owner's hand.", 'Equip {5}'): ('return',4,4,5),
    ('Equipped creature gets +X/+X, where X is the number of creature cards in all graveyards.',
     'Equip {2}'): ('graveyard',0,0,2),
    ('Equipped creature gets +1/+1 for each Swamp you control.',
     'Equip {B/P}{B/P} ({B/P} can be paid with either {B} or 2 life.)'): ('swamp',0,0,0),
    ('Equipped creature gets +0/+1 and has "Sacrifice this creature: This creature deals 1 damage to any target."',
     'Equip {2}'): ('sacrifice',0,1,2),
    ('Equipped creature gets +1/+1.',
     'Whenever equipped creature deals combat damage to a player, that player loses half their life, rounded up.',
     'Equip {3}'): ('halving',1,1,3),
}
RIDER_COMMON = ('enter','cast_short','countered','move','remove_source','bounce_germ','equip_target_leaves',
                'replay_entry','replay_move')
RIDER_MODES = {
    'return': ('return','return_short','return_offturn','return_foreign','return_host','return_recast',
               'germ_combat','host_combat','blocked_combat'),
    'graveyard': ('own_grave_only','foe_grave_only','zero_count','grave_grow','grave_shrink','grave_to_zero'),
    'swamp': ('zero_count','own_nonbasic','swamp_grow','swamp_to_zero','equip_black','equip_mixed','equip_life','equip_insufficient'),
    'sacrifice': ('ping_player','ping_self','ping_creature','ping_lethal','ping_walker','ping_host','ping_lifelink',
                  'ping_replay','no_host_equip_short'),
    'halving': ('even_remainder','odd_remainder','blocked_combat','walker_combat','noncombat','germ_combat','combat_replay'),
}


def rider_parameters(record, lines):
    from recipes import cost_from_oracle

    shape = RIDER_EFFECTS.get(tuple(lines[1:])) if lines and lines[0]==LIVING_WEAPON else None
    if shape is None:
        return None
    rider,power,toughness,equip = shape
    return dict(effect='living_weapon',rider=rider,cast_mana=cost_from_oracle(record),equip_generic=equip,
                power_delta=power,toughness_delta=toughness,keyword='')


def validate_riders(oracle,rows):
    for group in ('rider_sources','rider_witnesses'):
        for name,pin in pins()[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or row.get('kind')!='card'
                    or row.get('handler') in (None,'NULL','0','card_dummy') or record.get('layout')!='normal'):
                raise ValueError(f'implemented Living Weapon rider input changed: {name}')


class LivingWeaponRiderScene(LivingWeaponScene):
    def __init__(self,*args):
        super().__init__(*args)
        self.rider = self.model['rider']
        self.loyalty = None

    def bonus(self):
        if self.rider=='graveyard':
            n = sum(e['zone']=='graveyard' and 'Creature' in self.oracle[e['name']]['type_line'].split(' — ',1)[0]
                    for e in self.entities.values())
            return n,n
        if self.rider=='swamp':
            controller = self.entities['source']['controller']
            n = sum(e['zone']=='battlefield' and e['controller']==controller and
                    'Swamp' in self.oracle[e['name']]['type_line'].partition(' — ')[2].split() for e in self.entities.values())
            return n,n
        return self.model['power_delta'],self.model['toughness_delta']

    def stats(self,ref):
        p,t = TeamScene.stats(self,ref)
        if self.attached==ref:
            dp,dt = self.bonus()
            return p+dp,t+dt
        return p,t

    def receipts(self):
        super().receipts()
        if self.loyalty is not None and self.entities['walker']['zone']=='battlefield':
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.loyalty)

    def germ_absent(self):
        self.emit('assert_zone_multiplicity',player=self.owner,zone='battlefield',index=GERM_ID,value=0)
        self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=GERM_ID,value=0)

    def germ_died(self):
        if 'watcher' in self.entities:
            self.life[self.owner] += 1
            self.life[self.foe] -= 1
        self.germ_absent()

    def create_germ(self):
        if self.bonus()[1]>0:
            super().create_germ()
        else:
            # Real Living Weapon created the Germ. Its fatal toughness is observed through Zulaport.
            self.germ_died()

    def setup_graves(self):
        m = self.mode
        dynamic = m in ('grave_grow','grave_shrink','grave_to_zero')
        if m not in ('zero_count','foe_grave_only','grave_to_zero'):
            self.add('Catapult Squad','own_grave',self.owner,'graveyard')
        if m not in ('zero_count','own_grave_only','grave_grow'):
            self.add('Mogg Fanatic','foe_grave',self.foe,'graveyard')
        self.add('Mountain','noncreature_grave',self.owner,'graveyard')
        if dynamic and m!='grave_grow':
            self.add("Tormod's Crypt",'crypt',self.owner)
        if m=='grave_grow':
            self.add('Shock','shock',self.owner,'hand')
        if m in ('zero_count','grave_to_zero'):
            self.add('Zulaport Cutthroat','watcher',self.owner)

    def setup_swamps(self):
        m = self.mode
        # Keep real land mana out of payment cells. Oracle counts tapped Swamps as well.
        if m!='zero_count':
            self.add('Bayou' if m=='own_nonbasic' else 'Swamp','own_swamp',self.owner,tapped=True)
        self.add('Forest','own_forest',self.owner,tapped=True)
        self.add('Swamp','foe_swamp',self.foe,tapped=True)
        if m=='swamp_grow':
            self.add('Swamp','extra_swamp',self.owner,'hand')
        if m=='swamp_to_zero':
            self.add('Boomerang','swamp_bounce',self.foe,'hand')
        if m in ('zero_count','swamp_to_zero'):
            self.add('Zulaport Cutthroat','watcher',self.owner)

    def setup_cards(self):
        super().setup_cards()
        m = self.mode
        if m in ('ping_host','ping_lifelink','noncombat'):
            # Replace the single inert ledger witness before any operation is generated.
            name = {'ping_host':'Mogg Fanatic','ping_lifelink':'Child of Night','noncombat':'Prodigal Sorcerer'}[m]
            self.setup = [x for x in self.setup if x['ref']!='ally']
            del self.entities['ally']
            self.add(name,'ally',self.owner)
        if self.rider=='graveyard':
            self.setup_graves()
        if self.rider=='swamp':
            self.setup_swamps()
        if m=='return_foreign':
            self.add('Bazaar Trader','trader',self.owner)
        if m=='ping_lethal' or m=='grave_grow':
            self.add('Mogg Fanatic','victim',self.foe)
        if m in ('ping_walker','walker_combat'):
            self.add('Jace Beleren','walker',self.foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])

    def prepare(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        if self.mode in ('equip_black','equip_insufficient','even_remainder','odd_remainder'):
            actor = self.foe if self.mode.endswith('remainder') else self.owner
            number = {'even_remainder':20,'odd_remainder':19}.get(self.mode,1)
            self.life[actor] = number
            self.emit('set_life',player=actor,value=number)
        self.emit('refresh')

    def begin_rider_equip(self,*,denied=False,short=False):
        actor = self.entities['source']['controller']
        price = self.model['equip_generic']
        black,life_symbols = 0,0
        if self.rider=='swamp':
            black,life_symbols = {'equip_black':(2,0),'equip_mixed':(1,1),'equip_insufficient':(0,0)}.get(self.mode,(0,2))
        self.fund(actor,(price-int(short),black,0,0,0,0))
        self.pools()
        if self.mode=='replay_move':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(not denied))
        if not denied:
            if self.rider in ('return','sacrifice'):
                self.emit('choose_dialog',player=actor,value=0)
            if self.rider=='swamp' and self.mode!='equip_black':
                self.emit('choose_number',player=actor,value=life_symbols)
            self.emit('choose_target',player=actor,ref='ally')
        self.emit('activate',ref='source',value=int(not denied))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(not denied))
        if denied:
            self.receipts()
            return False
        self.pay(actor,(price,black,0,0,0,0))
        self.life[actor] -= 2*life_symbols
        self.pools()
        self.emit('assert_life',player=actor,value=self.life[actor])
        self.prop('source','tapped',0)
        for ref in (self.attached,'ally'):
            if ref:
                for key,value in zip(('power','toughness'),self.stats(ref)):
                    self.prop(ref,key,value)
        return True

    def equip_ally(self):
        if self.begin_rider_equip(denied=self.mode=='equip_insufficient'):
            self.finish_equip('ally')

    def update_count(self):
        m = self.mode
        if m=='grave_grow':
            self.spell('shock','victim')
            self.resolve()
            self.leave('shock','graveyard')
            self.leave('victim','graveyard')
        elif m in ('grave_shrink','grave_to_zero'):
            self.emit('choose_player',player=self.owner,value=self.foe)
            self.emit('activate',ref='crypt',value=1)
            self.emit('assert_choices_consumed')
            self.emit('assert_stack',value=1)
            self.leave('crypt','graveyard')
            self.resolve()
            self.leave('foe_grave','exile')
        elif m=='swamp_grow':
            self.emit('play_land',ref='extra_swamp',value=1)
            self.entities['extra_swamp']['zone'] = 'battlefield'
        else:
            self.spell('swamp_bounce','own_swamp')
            self.resolve()
            self.leave('swamp_bounce','graveyard')
            self.leave('own_swamp','hand')
        self.reap()
        if m in ('grave_to_zero','swamp_to_zero'):
            self.germ_died()
        self.receipts()

    def return_equipment(self):
        m = self.mode
        if m=='return_host':
            self.equip_ally()
        if m=='return_foreign':
            self.donate('source')
        if m in ('return_offturn','return_foreign'):
            self.advance()
        actor = self.entities['source']['controller']
        allowed = m!='return_short'
        self.fund(actor,(3 if allowed else 2,0,0,0,0,0))
        self.emit('can_activate',ref='source',value=int(allowed))
        if allowed:
            self.emit('choose_dialog',player=actor,value=1)
        self.emit('activate',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pay(actor,(3,0,0,0,0,0))
            self.pools()
            self.prop('source','in_play',1)
            if self.attached:
                self.prop(self.attached,'toughness',self.stats(self.attached)[1])
            self.resolve()
            lost_germ = self.attached=='germ'
            self.leave('source','hand')
            self.reap()
            if lost_germ:
                self.germ_absent()
        self.receipts()
        if m=='return_recast':
            self.emit('bind_hand_copy',ref='source',player=self.owner,index=0,value=self.csvid)
            self.cast_equipment()

    def ping(self):
        m = self.mode
        if m in ('ping_host','ping_lifelink'):
            self.equip_ally()
        host = self.attached
        target = {'ping_creature':'enemy','ping_lethal':'victim','ping_walker':'walker'}.get(m)
        recipient = self.owner if m=='ping_self' else self.foe
        if m=='ping_replay':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=1)
        if self.owner==1:
            self.emit('choose_dialog',player=self.owner,value=1)
        if target:
            self.emit('choose_target',player=self.owner,ref=target)
        else:
            self.emit('choose_player',player=self.owner,value=recipient)
        self.emit('activate',ref='source',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.leave(host,'graveyard')
        self.emit('refresh')
        if host=='germ':
            self.germ_absent()
        else:
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=self.observed_id(host),value=1)
        for actor,n in enumerate(self.life):
            self.emit('assert_life',player=actor,value=n)
        self.resolve()
        if target=='walker':
            self.loyalty -= 1
        elif target:
            self.entities[target]['damage'] += 1
            self.reap(damage=True)
        else:
            self.life[recipient] -= 1
        if m=='ping_lifelink':
            self.life[self.owner] += 1
        self.receipts()
        if m=='no_host_equip_short':
            self.begin_rider_equip(denied=True,short=True)

    def combat_rider(self):
        m = self.mode
        attacker = 'germ' if m=='germ_combat' or self.rider=='return' and m=='blocked_combat' else 'ally'
        if attacker=='ally':
            self.equip_ally()
        else:
            self.advance()
            self.advance()
        if m=='combat_replay':
            self.replay = len(self.steps)
        self.emit('begin_attackers')
        if m=='walker_combat':
            self.emit('choose_dialog',player=self.owner,value=1)
            self.emit('attack_with_tax',ref=attacker,value=1)
        else:
            self.emit('attack',ref=attacker,value=1)
        vigilance = self.rider=='return'
        self.entities[attacker]['tapped'] = not vigilance
        self.emit('finish_attackers')
        self.prop(attacker,'tapped',int(not vigilance))
        self.emit('begin_blockers')
        blocked = m=='blocked_combat'
        if blocked:
            self.emit('block',ref='enemy',other=attacker,value=1)
        self.emit('advance_combat')
        if m=='walker_combat':
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')
            self.loyalty -= self.damage_power(attacker)
            self.receipts()
            return
        damage = self.damage_power(attacker)
        if self.rider=='return':
            self.life[self.owner] += damage
        elif not blocked:
            remaining = self.life[self.foe]-damage
            self.life[self.foe] -= (remaining+1)//2
        self.combat_finish((attacker,),{attacker:'enemy'} if blocked else {})

    def noncombat(self):
        self.equip_ally()
        self.emit('choose_player',player=self.owner,value=self.foe)
        self.emit('activate',ref='ally',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.entities['ally']['tapped'] = True
        self.resolve()
        self.life[self.foe] -= 1
        self.receipts()

    def play(self):
        self.prepare()
        if not self.cast_equipment():
            return
        m = self.mode
        if m in ('move','equip_target_leaves','replay_move','equip_black','equip_mixed','equip_life','equip_insufficient'):
            self.equip_ally()
        elif m in ('remove_source','bounce_germ'):
            self.follow_entry()
        elif m.startswith('return'):
            self.return_equipment()
        elif m in ('grave_grow','grave_shrink','grave_to_zero','swamp_grow','swamp_to_zero'):
            self.update_count()
        elif m.startswith('ping_') or m=='no_host_equip_short':
            self.ping()
        elif m=='noncombat':
            self.noncombat()
        elif m in ('germ_combat','host_combat','blocked_combat','odd_remainder','even_remainder','walker_combat','combat_replay'):
            self.combat_rider()


def expand_rider(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_riders(oracle,rows)
    if csvid!=pins()['rider_sources'][name]['registry']['csvid'] or set(selected)-set(variants(model)):
        raise ValueError('unreserved Living Weapon rider or variant')
    cases = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        scene = LivingWeaponRiderScene(name,model,mode,int(seat),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            cut = scene.replay
            suffix = steps[cut:]
            steps = steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        gaps = ['Direct Germ subtype query beyond native identity and Phyrexian','Direct attachment-pointer observation','Multiplayer']
        if model['rider']=='sacrifice':
            gaps += ['Battle targets','Color-based last-known source attribution']
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
        cases.append(dict(name=f'LivingWeaponEquipmentRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2008,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
