"""Real face-down casts, paid Morph, and printed battlefield riders."""
import copy
import json
from pathlib import Path

from library_search_recipes import wave6_exact
from morph_creature_recipes import FACE_DOWN, MORPH
from morph_with_keywords_recipes import _World


FAMILY = 'morph_battlefield_riders'
PROFILE = Path(__file__).parent / 'data/morph_battlefield_riders_wave8_profiles.json'
GRAMMAR = {
    ('At the beginning of your upkeep, you lose 2 life.',): ('demon', 'Creature — Demon', ()),
    ('Flying, trample', "At the beginning of your upkeep, sacrifice a creature other than this creature. If you can't, this creature deals 7 damage to you."):
        ('liege', 'Creature — Demon', ('Flying', 'Trample')),
    ('Whenever another nontoken creature you control dies, draw a card.',):
        ('haruspex', 'Creature — Human Wizard', ()),
    ('Creatures you control get +0/+1.',): ('field', 'Enchantment', ()),
    ('Damage that would reduce your life total to less than 1 reduces it to 1 instead.',):
        ('thief', 'Creature — Human Rogue', ()),
}
HANDLERS = {'demon':'manalinkeh_card_grinning_demon', 'liege':'manalinkeh_card_liege_of_the_pit',
            'haruspex':'manalinkeh_card_grim_haruspex', 'field':'manalinkeh_card_lumithread_field',
            'thief':'manalinkeh_card_fortune_thief'}
WITNESSES = {'Forest', 'Mountain', 'Plains', 'Grizzly Bears', 'Hill Giant', 'Wind Drake', 'Shock',
             'Unsummon', 'Swords to Plowshares', 'Raise the Alarm', 'Soldier', 'Expedition Map',
             'Disenchant', 'Boomerang', 'Pyroclasm', 'Lightning Bolt', 'Bump in the Night'}
DUMMIES = {'Grizzly Bears','Hill Giant','Wind Drake'}
OPENING = ('printed_cast','hidden_cast','hidden_choice','cast_short','reveal_ready','reveal_short',
           'wrong_color','offturn_reveal','reveal_replay')
HIDDEN = {'hidden_cast','hidden_choice','hidden_upkeep','hidden_death','hidden_damage','hidden_lethal'}
EXTRAS = {
    'demon': ('own_upkeep','opposing_upkeep','hidden_upkeep','double_upkeep','printed_effect'),
    'liege': ('own_upkeep','opposing_upkeep','hidden_upkeep','double_upkeep','printed_effect',
              'own_fodder','opponent_only','artifact_only','trample_hit','flying_evasion'),
    'haruspex': ('printed_effect','own_death','hidden_death','foreign_death','token_death',
                 'self_death','self_and_other','artifact_death','bounced','exiled'),
    'field': ('shore_up','hidden_damage','destroy_field','bounce_field','target_type','printed_effect'),
    'thief': ('floor','printed_effect','nonlethal','exact_one','at_one','hidden_lethal',
              'combat_floor','other_player','life_loss','source_absent'),
}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import COLORS, cost_from_oracle

    if not record or record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal Morph permanent required')
    lines = record['oracle_text'].splitlines()
    fees = [(i, MORPH.fullmatch(line)) for i,line in enumerate(lines) if MORPH.fullmatch(line)]
    if len(fees) != 1:
        raise ValueError('exactly one printed Morph cost needed')
    (index, fee), = fees
    rule = GRAMMAR.get(tuple(lines[:index]+lines[index+1:]))
    if not rule:
        raise ValueError('complete reviewed battlefield rider required')
    mode, type_line, keys = rule
    if record['type_line'] != type_line or sorted(record['keywords']) != sorted(['Morph', *keys]):
        raise ValueError('printed type/keyword disagreement')
    mana = cost_from_oracle(record)
    morph = cost_from_oracle(dict(record, mana_cost=fee[1]))
    if not any(morph[1:]) or sum(mana)<2:
        raise ValueError('reviewed exact coloured costs required')
    stats = (record.get('power'),record.get('toughness'))
    if mode == 'field':
        if stats != (None,None):
            raise ValueError('printed Enchantment has no P/T')
        power = toughness = None
    else:
        if any(type(stat) is not str or not stat.isascii() or not stat.isdecimal() for stat in stats):
            raise ValueError('printed fixed creature P/T')
        power,toughness = map(int,stats)
    return dict(mode=mode,mana=mana,morph=morph,power=power,toughness=toughness,keywords=list(keys),
                color=sum(1<<COLORS[name] for name in record['colors']))


def variants(shape):
    return [f'{scenario}_{owner}' for scenario in OPENING+EXTRAS[shape['mode']] for owner in (0,1)]


def spec(declaration, record):
    shape = classify(record)
    source = profiles()['sources'].get(record['name'])
    names = declaration.get('variants')
    if (not source or not wave6_exact(source['oracle'],record) or declaration.get('family') != FAMILY
            or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['oracle_text'] or not wave6_exact(declaration.get('params'),shape)
            or not isinstance(names,list) or not names or len(set(names))!=len(names) or set(names)-set(variants(shape))):
        raise ValueError('exact pinned permanent, printed rider and distinct cases required')
    return dict(shape,family=FAMILY,effect=FAMILY)


def validate_witnesses(oracle,rows):
    data = profiles()
    if (set(data)!={'sources','witnesses','face_down'} or set(data['witnesses'])!=WITNESSES
            or data['face_down']['csvid']!=FACE_DOWN or rows.get(data['face_down']['name'])!=data['face_down']):
        raise ValueError('complete reviewed battlefield witnesses required')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            row=rows[name]
            if (not wave6_exact(pin['oracle'],oracle[name]) or pin['registry']!=row or row['kind']!='card'
                    or row['handler'] in (None,'NULL','0') or (row['handler']=='card_dummy' and name not in DUMMIES|{'Soldier'})):
                raise ValueError('out-of-scope original or witness: '+name)
            if group=='sources' and row['handler'] != HANDLERS[classify(oracle[name])['mode']]:
                raise ValueError('wrong handler for pinned full rule: '+name)


class Battlefield(_World):
    def __init__(self,name,shape,scenario,owner,rows,oracle):
        super().__init__(name,shape,owner,rows,oracle)
        self.mode,self.scenario=shape['mode'],scenario
        self.field_active=False
        self.created=()
        self.add(name,'source',source=True)
        self.witnesses()

    def witnesses(self):
        mode,case,owner,foe=self.mode,self.scenario,self.owner,self.foe
        if mode=='liege':
            if case in ('own_fodder','double_upkeep','trample_hit','flying_evasion','hidden_upkeep','opposing_upkeep'):
                self.add('Grizzly Bears','fodder',owner,'battlefield')
            if case=='opponent_only':
                self.add('Grizzly Bears','foreign',foe,'battlefield')
            if case=='artifact_only':
                self.add('Expedition Map','map',owner,'battlefield')
            if case in ('trample_hit','flying_evasion'):
                self.add('Wind Drake' if case=='trample_hit' else 'Hill Giant','blocker',foe,'battlefield')
        elif mode=='haruspex':
            if case in ('printed_effect','own_death','hidden_death','foreign_death','self_and_other','bounced','exiled'):
                self.add('Grizzly Bears','victim',foe if case=='foreign_death' else owner,'battlefield')
            if case in ('printed_effect','own_death','hidden_death','foreign_death','self_death','token_death'):
                self.add('Shock','answer',foe)
            if case=='self_and_other':
                self.add('Pyroclasm','answer',owner)
            if case=='token_death':
                self.add('Raise the Alarm','create',owner)
            if case=='artifact_death':
                self.add('Expedition Map','victim',owner,'battlefield')
                self.add('Disenchant','answer',foe)
            if case in ('bounced','exiled'):
                self.add('Unsummon' if case=='bounced' else 'Swords to Plowshares','answer',foe)
        elif mode=='field':
            self.add('Grizzly Bears','ally',owner,'battlefield')
            self.add('Hill Giant','opposing',foe,'battlefield')
            if case in ('shore_up','hidden_damage'):
                self.add('Shock','answer',foe)
            if case in ('destroy_field','target_type'):
                self.add('Disenchant','answer',foe)
            if case=='bounce_field':
                self.add('Boomerang','answer',foe)
        elif mode=='thief':
            if case=='combat_floor':
                self.add('Hill Giant','attacker',foe,'battlefield')
            elif case=='life_loss':
                self.add('Bump in the Night','answer',foe)
            elif case not in OPENING:
                self.add('Lightning Bolt','answer',owner if case=='other_player' else foe)
            if case=='source_absent':
                self.add('Shock','removal',foe)
        for player in (0,1):
            library=('Forest','Mountain','Plains')
            for i,label in enumerate(library):
                self.add(label,f'library_{player}_{i}',player,'library')

    def identity(self):
        if self.mode=='field' and not self.hidden:
            self.observe('source','csvid',self.rows[self.name]['csvid'])
        else:
            super().identity()

    def toggle_field(self,active):
        if self.field_active != active:
            self.field_active=active
            for thing in self.entities.values():
                if thing['zone']=='battlefield' and thing['player']==self.owner:
                    thing['toughness'] += 1 if active else -1

    def move(self,ref,zone):
        old=self.cards[ref][2]
        super().move(ref,zone)
        if self.mode=='field' and ref=='source' and not self.hidden and old!=zone:
            self.toggle_field(zone=='battlefield')

    def source_cast(self,*,hidden,choice=False,short=False):
        if self.mode!='field':
            return super().source_cast(hidden=hidden,choice=choice,short=short)
        colored=self.p['mana'][5]
        print_cost=self.p['mana']
        fee=[3,0,0,0,0,0] if hidden else print_cost
        budget=[2,0,0,0,0,0] if short else [3,0,0,0,0,colored] if choice else fee
        self.pending('source',allowed=not short,price=budget,spend=fee,dialog=0 if choice else None)
        if short:
            return
        self.hidden=hidden
        if hidden:
            self.entities['source']=dict(name=self.name,player=self.owner,zone='hand',source=True,
                token=False,power=2,toughness=2,sick=False,tapped=False,attacking=False,damage=0,keys=set(),grants=set())
        self.identity()

    def reveal(self,*,denials=None):
        if self.mode!='field':
            return super().reveal(denials=denials)
        cost=list(self.p['morph'])
        if denials=='reveal_short':
            cost[0]-=1
        elif denials=='wrong_color':
            cost[5],cost[2]=0,1
        self.pool(self.owner,cost)
        self.emit('refresh')
        allowed=denials is None
        self.emit('can_activate',ref='source',value=int(allowed))
        self.emit('activate',ref='source',value=int(allowed))
        if allowed:
            self.hidden=False
            del self.entities['source']
            self.toggle_field(True)
            self.mana[self.owner]=[0]*6
            self.emit('refresh')
            self.identity()
            self.wallets()
            self.emit('resolve')
        self.emit('assert_unused_choices',value=0)

    def expire(self):
        if self.mode=='field' and 'source' not in self.entities:
            for item in self.entities.values():
                item.update(damage=0,attacking=False,grants=set())
            self.fog=False
        else:
            super().expire()

    def receipts(self):
        super().receipts()
        if self.created:
            for ref in self.created:
                if self.cards[ref][2]=='battlefield':
                    self.emit('assert_matching_permanents',player=self.owner,index=self.rows['Soldier']['csvid'],property='token',value=1)
                    break
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=self.rows['Soldier']['csvid'],value=0)

    def visit_upkeep(self):
        self.emit('advance_turn')
        self.expire()
        self.current=1-self.current
        self.emit('untap_step',player=self.current)
        for item in self.entities.values():
            if item['zone']=='battlefield' and item['player']==self.current:
                item.update(tapped=False,sick=False)
        if self.current==self.owner and not self.hidden:
            if self.mode=='demon':
                self.life[self.owner]-=2
            elif 'fodder' in self.cards and self.cards['fodder'][2]=='battlefield':
                if self.owner==0:
                    self.emit('choose_target',player=0,ref='fodder')
                self.move('fodder','graveyard')
            else:
                self.life[self.owner]-=7
        self.emit('upkeep_step',player=self.current)
        self.receipts()
        self.emit('turn',player=self.current,value=0)

    def upkeep_rider(self):
        self.visit_upkeep()
        if self.scenario!='opposing_upkeep':
            self.visit_upkeep()
        if self.scenario=='double_upkeep':
            self.visit_upkeep()
            self.visit_upkeep()
        if self.scenario in ('trample_hit','flying_evasion'):
            self.fight('source','blocker',allowed_block=self.scenario=='trample_hit')

    def draw_one(self):
        self.zones['hand'][self.owner].append(self.zones['library'][self.owner].pop(0))

    def remove_by(self,spell,ref,zone='graveyard'):
        self.pending(spell,target=ref)
        self.move(ref,zone)
        self.finish_spell(spell,'graveyard')

    def prepare_soldiers(self):
        self.pending('create')
        self.emit('resolve')
        self.move('create','graveyard')
        self.created=('first_soldier','second_soldier')
        for number,ref in enumerate(self.created):
            self.entity('Soldier',ref,self.owner,'battlefield',token=True)
            self.cards[ref]=('Soldier',self.owner,'battlefield')
            self.zones['battlefield'][self.owner].append('Soldier')
            self.emit('bind_permanent_copy',ref=ref,player=self.owner,index=number,value=self.rows['Soldier']['csvid'])
        self.receipts()

    def death_rider(self):
        case=self.scenario
        if case=='token_death':
            self.prepare_soldiers()
            self.remove_by('answer','first_soldier')
        elif case=='self_and_other':
            self.pending('answer')
            self.move('source','graveyard')
            self.move('victim','graveyard')
            self.draw_one()
            self.finish_spell('answer','graveyard')
        elif case=='exiled':
            self.life[self.owner]+=self.entities['victim']['power']
            self.remove_by('answer','victim','exile')
        elif case=='bounced':
            self.remove_by('answer','victim','hand')
        elif case=='self_death':
            self.remove_by('answer','source')
        else:
            draws=case in ('printed_effect','own_death')
            if draws:
                self.draw_one()
            self.remove_by('answer','victim')

    def field_rider(self):
        case=self.scenario
        if case in ('shore_up','hidden_damage'):
            self.pending('answer',target='ally')
            if self.hidden:
                self.move('ally','graveyard')
            else:
                self.entities['ally']['damage']=2
            self.finish_spell('answer','graveyard')
        elif case in ('destroy_field','target_type'):
            self.remove_by('answer','source')
        elif case=='bounce_field':
            self.remove_by('answer','source','hand')

    def player_spell(self,ref,recipient,amount,floor):
        from recipes import cost_from_oracle

        name,actor,_=self.cards[ref]
        self.pool(actor,cost_from_oracle(self.o[name]))
        self.emit('refresh')
        # Bump in the Night binds the sole opposing player in the actual
        # two-player spell callback without offering a selector.
        if name!='Bump in the Night':
            self.emit('choose_player',player=actor,value=recipient)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.mana[actor]=[0]*6
        after=self.life[recipient]-amount
        self.life[recipient]=max(1,after) if floor else after
        self.finish_spell(ref,'graveyard')

    def damage_rider(self):
        case=self.scenario
        player=self.foe if case=='other_player' else self.owner
        before={'nonlethal':8,'exact_one':4,'at_one':1}.get(case,2)
        self.life[player]=before
        self.emit('set_life',player=player,value=before)
        if case=='source_absent':
            self.remove_by('removal','source')
        if case in ('combat_floor','life_loss'):
            self.next_turn()
        if case=='combat_floor':
            self.emit('begin_attackers')
            self.emit('attack',ref='attacker',value=1)
            self.entities['attacker'].update(attacking=True,tapped=True)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.emit('resolve')
            self.emit('end_combat')
            self.life[self.owner]=1
            self.entities['attacker']['attacking']=False
            self.receipts()
        else:
            self.player_spell('answer',player,3,case not in ('hidden_lethal','other_player','life_loss','source_absent'))


def build(name,csvid,declaration,shape,variant,rows,oracle):
    from recipes import cost_from_oracle,step

    scenario,seat=variant.rsplit('_',1)
    owner=int(seat)
    world=Battlefield(name,shape,scenario,owner,rows,oracle)
    world.emit('turn',player=owner,value=0)
    for player in (0,1):
        world.pool(player,[0]*6)
    if scenario=='cast_short':
        world.source_cast(hidden=True,short=True)
        world.receipts()
    else:
        world.source_cast(hidden=scenario not in ('printed_cast','printed_effect'),choice=scenario=='hidden_choice')
        world.finish_spell('source','battlefield')
        if scenario=='offturn_reveal':
            world.next_turn()
        if scenario=='target_type':
            world.pool(world.foe,cost_from_oracle(oracle['Disenchant']))
            world.emit('refresh')
            world.emit('can_cast',ref='answer',value=0)
        cut=len(world.steps)
        if scenario not in HIDDEN | {'printed_cast','printed_effect'}:
            world.reveal(denials=scenario if scenario in ('reveal_short','wrong_color') else None)
            world.receipts()
        if scenario=='reveal_replay':
            suffix=copy.deepcopy(world.steps[cut:])
            world.steps=(world.steps[:cut]+[step('capture')]+suffix+
                        [step('record_state'),step('ai_roundtrip'),step('restore')]+copy.deepcopy(suffix)+[step('assert_recorded_state')])
        if scenario not in OPENING:
            if scenario=='target_type':
                world.emit('can_cast',ref='answer',value=1)
            if shape['mode'] in ('demon','liege'):
                world.upkeep_rider()
            elif shape['mode']=='haruspex':
                world.death_rider()
            elif shape['mode']=='field':
                world.field_rider()
            else:
                world.damage_rider()
    gaps=['Independent intervention within the automatic production trigger callback; multiplayer.']
    if shape['mode']=='liege':
        gaps.append('AI mandatory sacrifice uses an unambiguous legal creature; observed damage is separate from nondamage life loss.')
    if shape['mode']=='thief':
        gaps.append('The selected damage comparisons do not change the damage source or run through poison replacement effects.')
    return dict(name=f'MorphBattlefieldRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
      setup=world.setup,steps=world.steps,_recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
      oracle_text=declaration['oracle_text'],untested_components=gaps))


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('exact approved distinct battlefield Morph scenarios required')
    validate_witnesses(oracle,rows)
    return [build(name,csvid,declaration,shape,variant,rows,oracle) for variant in selected]
