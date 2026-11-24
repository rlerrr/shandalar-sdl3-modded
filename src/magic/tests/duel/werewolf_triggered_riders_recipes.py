"""Actual transform histories and independently observed effects on both faces."""

import copy
import json
from pathlib import Path

from library_search_recipes import wave6_exact
from werewolf_spell_history_recipes import Board as HistoryBoard, DECK, SMALL, TRIGGERS


FAMILY = 'werewolf_triggered_riders'
PROFILE = Path(__file__).parent / 'data/werewolf_triggered_riders_wave8_profiles.json'
PATTERNS = {
    ((), ("Whenever this creature transforms into Werewolf Ransacker, you may destroy target artifact. If that artifact is put into a graveyard this way, this creature deals 3 damage to that artifact's controller.",)): 'deserter',
    (('Whenever this creature enters or transforms into Huntmaster of the Fells, create a 2/2 green Wolf creature token and you gain 2 life.',),
     ('Trample', 'Whenever this creature transforms into Ravager of the Fells, it deals 2 damage to target opponent or planeswalker and 2 damage to up to one target creature that player or that planeswalker’s controller controls.'.replace('’', "'"))): 'huntmaster',
    ((), ('Whenever this creature deals combat damage to a player, draw a card.',)): 'elder',
    (('Other Human creatures you control get +1/+1.',),
     ("Each other creature you control that's a Werewolf or a Wolf gets +1/+1.", 'At the beginning of your end step, create a 2/2 green Wolf creature token.')): 'mayor',
    ((), ('Whenever an opponent casts a spell, this creature deals 2 damage to that player.',)): 'magehunter',
    (('Haste',), ('Whenever a Werewolf you control enters, you may transform it.',)): 'bandit',
    (('Whenever an opponent casts a spell during your turn, draw a card.',),
     ('Whenever an opponent casts a spell during your turn, draw two cards.',)): 'hermit',
    (('Whenever this creature blocks or becomes blocked by a creature, this creature deals 1 damage to that creature.',),
     ('Whenever this creature blocks or becomes blocked by a creature, this creature deals 2 damage to that creature.',)): 'kessig',
}
COMMON = ('short_cast', 'split_stays', 'replay')
EXTRA = {
    'deserter': ('enemy_artifact', 'own_artifact', 'decline', 'indestructible'),
    'huntmaster': ('entry', 'flip_back_pair', 'walker', 'no_creature', 'decline_creature', 'trample'),
    'elder': ('front_hit', 'back_hit', 'blocked', 'no_hit'),
    'mayor': ('buffs', 'back_tokens', 'own_end_again', 'revert_lords'),
    'magehunter': ('front_foe', 'back_owner', 'back_enemy', 'offturn_enemy', 'countered'),
    'bandit': ('haste', 'own_wolf', 'other_wolf', 'not_werewolf', 'decline'),
    'hermit': ('front', 'back', 'not_your_turn', 'own_spell', 'countered'),
    'kessig': ('front_blocks', 'front_blocked', 'back_blocks', 'back_blocked', 'unblocked'),
}
WITNESSES = set(DECK) | {SMALL, 'Counterspell', 'Darksteel Relic', 'Elite Vanguard', 'Fog', 'Grizzly Bears',
                       'Howlpack of Estwald', 'Jace Beleren', 'Memnite', 'Villagers of Estwald', 'Wolf',
                       'Woodland Changeling', 'Young Wolf'}
STATIC = {SMALL, 'Elite Vanguard', 'Grizzly Bears', 'Memnite', 'Woodland Changeling'}
EXTRA_PAIR = ('Villagers of Estwald', 'Howlpack of Estwald')
HANDLED_ARTIFACT = ('enemy_artifact', 'own_artifact', 'decline', 'indestructible')
HUNT_CREATURE = ('flip_back_pair', 'walker', 'decline_creature', 'trample')
MAYOR_WITNESSES = ('buffs', 'back_tokens', 'own_end_again', 'revert_lords')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(front, back=None):
    from recipes import COLORS, cost_from_oracle

    faces = front.get('card_faces', [])
    if (front.get('layout') != 'transform' or len(faces) != 2
            or front.get('engine_face') != faces[0].get('name')):
        raise ValueError('physical front of a two-face transform card required')
    a, b = faces
    back = profiles()['sources'][a['name']]['back_oracle'] if back is None else back
    if (a['name'] + ' // ' + b['name'] != front.get('name') or back.get('name') != front['name']
            or back.get('engine_face') != b['name'] or back.get('layout') != 'transform'
            or back.get('oracle_id') != front.get('oracle_id') or not wave6_exact(back.get('card_faces'), faces)
            or not a['mana_cost'] or b['mana_cost'] or b.get('color_indicator') != b['colors']):
        raise ValueError('complete linked transform pair required')
    shapes, printed = [], []
    fields = ('name', 'oracle_text', 'mana_cost', 'type_line', 'power', 'toughness', 'colors')
    for raw, record, last in zip(faces, (front,back), TRIGGERS):
        selected = {key: raw[key] for key in fields}
        if (not wave6_exact(selected, record.get('selected_face'))
                or not raw['type_line'].startswith('Creature — ') or 'Werewolf' not in raw['type_line'].split()
                or not all(raw[k].isascii() and raw[k].isdecimal() for k in ('power','toughness'))):
            raise ValueError('exact fixed physical face fields required')
        lines = raw['oracle_text'].splitlines()
        if lines[-1:] != [last]:
            raise ValueError('both printed spell-history predicates required')
        printed.append(tuple(lines[:-1]))
        shapes.append(dict(name=raw['name'],power=int(raw['power']),toughness=int(raw['toughness']),
                           color=sum(1<<COLORS[color] for color in raw['colors']),
                           keywords=[s for s in lines[:-1] if s in ('Trample','Haste')],clauses=lines))
    model = PATTERNS.get(tuple(printed))
    keywords = {'Transform', *shapes[0]['keywords'], *shapes[1]['keywords']}
    if not model or set(front['keywords']) != keywords or set(back['keywords']) != keywords:
        raise ValueError('both complete physical clauses and keywords required')
    return dict(model=model, price=cost_from_oracle(a), front=shapes[0], back=shapes[1])


def variants(shape):
    return [f'{mode}_{who}' for mode in COMMON+EXTRA[shape['model']] for who in (0,1)
            if (shape['model'],mode,who) != ('bandit','decline',1)]


def spec(declaration, record):
    pin = profiles()['sources'].get(record.get('engine_face'))
    if not pin or not wave6_exact(pin['oracle'], record):
        raise ValueError('exact whole Oracle primary required')
    shape = classify(record, pin['back_oracle'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or declaration.get('oracle_text') != record['selected_face']['oracle_text']
            or not wave6_exact(shape, declaration.get('params'))
            or not isinstance(chosen, list) or not chosen or any(not isinstance(v,str) for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('exact source and distinct reviewed rider variants required')
    return dict(shape, family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    if set(data) != {'sources','witnesses'} or set(data['witnesses']) != WITNESSES:
        raise ValueError('complete rider and witness pins required')
    for name, pin in data['witnesses'].items():
        row = rows[name]
        if (not wave6_exact(pin['oracle'], oracle[name]) or not wave6_exact(pin['registry'], row)
                or row['kind'] != 'card' or row['handler'] in ('NULL','0',None)
                or (row['handler'] == 'card_dummy' and name not in STATIC)):
            raise ValueError('witness Oracle record or registry changed: '+name)
    if rows['Wolf']['csvid'] != 18856 or oracle['Wolf']['layout'] != 'token':
        raise ValueError('expected canonical Wolf')
    if rows[EXTRA_PAIR[1]]['csvid'] != rows[EXTRA_PAIR[0]]['csvid'] + 1:
        raise ValueError('expected exact linked Werewolf entrant witness')
    for name, pin in data['sources'].items():
        front, back = pin['oracle'], pin['back_oracle']
        shape = classify(front, back)
        other_name = shape['back']['name']
        current, other = rows[name], rows[other_name]
        if (name != shape['front']['name'] or not wave6_exact(oracle[name], front)
                or not wave6_exact(oracle[other_name], back) or current != pin['registry'] or other != pin['back_registry']
                or current['kind'] != 'card' or other['kind'] != 'card' or other['csvid'] != current['csvid']+1
                or any(r['handler'] in ('card_dummy','NULL','0',None) for r in (current,other))):
            raise ValueError('expected live exact native faces: '+name)


class Riders(HistoryBoard):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        super().__init__(name, shape, mode, owner, rows, oracle)
        self.model = shape['model']
        self.additional_face = None
        self.wolves = 0
        self.loyalty = None
        if self.model == 'deserter' and mode in HANDLED_ARTIFACT:
            self.add('Darksteel Relic' if mode=='indestructible' else 'Memnite', 'artifact',
                     owner if mode=='own_artifact' else self.foe)
        if self.model == 'huntmaster' and mode in HUNT_CREATURE:
            self.add('Grizzly Bears', 'blocker', self.foe)
            if mode=='walker':
                self.add('Jace Beleren', 'walker', self.foe, 'hand')
        if self.model == 'elder' and mode=='blocked':
            self.add('Grizzly Bears', 'blocker', self.foe)
        if self.model == 'mayor' and mode in MAYOR_WITNESSES:
            for who in (owner,self.foe):
                for name, base in [('Elite Vanguard','human'),('Young Wolf','wolf'),('Woodland Changeling','changeling')]:
                    self.add(name, f'{base}_{who}', who)
            if mode=='buffs':
                self.add(EXTRA_PAIR[0], 'entrant', owner, 'hand')
        if self.model == 'kessig' and mode != 'unblocked':
            if mode in ('front_blocks','back_blocks'):
                self.add('Grizzly Bears', 'enemy_attacker', self.foe)
            elif mode in ('front_blocked','back_blocked'):
                self.add('Grizzly Bears', 'blocker', self.foe)
        if self.model == 'bandit' and mode in ('own_wolf','other_wolf','not_werewolf','decline'):
            self.add('Young Wolf' if mode=='not_werewolf' else EXTRA_PAIR[0], 'entrant',
                     self.foe if mode=='other_wolf' else owner, 'hand')
        if mode=='countered':
            self.add('Counterspell', 'counter', owner, 'hand')

    def fog_stock(self):
        mode, model = self.mode, self.shape['model']
        own = 2 if mode in ('replay','flip_back_pair','revert_lords') else int(
            mode in ('split_stays','back_owner','own_spell'))
        foe = int(mode in ('split_stays','front_hit','front_blocked','no_hit','front_foe',
                           'back_enemy','offturn_enemy','front','back','not_your_turn','countered'))
        if model == 'kessig' and mode in ('front_blocks', 'front_blocked'):
            # A real post-blockers Fog resolves the pending noncombat packet and
            # isolates its exact amount from the following combat assignment.
            own += mode == 'front_blocked'
            foe += mode == 'front_blocks'
        return [own if who == self.owner else foe for who in (0,1)]

    def identity(self, item):
        if item is self.cards.get('entrant') and self.additional_face and item['zone']=='battlefield':
            return self.rows[self.additional_face]['csvid']
        return super().identity(item)

    def stats(self, ref):
        if ref=='source':
            return super().stats(ref)
        item = self.cards[ref]
        name = self.additional_face if ref=='entrant' and self.additional_face else item['name']
        raw = self.oracle[name]
        raw = raw.get('selected_face', raw)
        if raw.get('power') is None:
            return 0,0
        subtypes = set(raw['type_line'].partition(' — ')[2].split())
        change = self.model=='mayor' and item['player']==self.owner and self.cards['source']['zone']=='battlefield'
        matches = name=='Woodland Changeling' or bool(subtypes & ({'Wolf','Werewolf'} if self.is_back else {'Human'}))
        bonus = int(change and matches)
        return int(raw['power'])+bonus,int(raw['toughness'])+bonus

    def receipts(self):
        super().receipts()
        if self.loyalty is not None:
            self.emit('assert_counter', ref='walker', counter_kind='loyalty', value=self.loyalty)
        if self.model in ('huntmaster','mayor'):
            for actor in (0,1):
                self.emit('assert_zone_multiplicity', player=actor, zone='battlefield', index=self.rows['Wolf']['csvid'],
                          value=self.wolves if actor==self.owner else 0)
        if self.cards.get('entrant',{}).get('zone')=='battlefield':
            self.emit('assert_card', ref='entrant', property='csvid', value=self.identity(self.cards['entrant']))

    def make_wolf(self):
        name='Wolf'
        self.emit('assert_zone_multiplicity', player=self.owner, zone='battlefield', index=self.rows[name]['csvid'],value=self.wolves+1)
        ref=f'token_{self.wolves}'
        self.emit('bind_permanent_copy', ref=ref, player=self.owner, index=self.wolves, value=self.rows[name]['csvid'])
        self.cards[ref]=dict(name=name,ref=ref,player=self.owner,zone='battlefield',tapped=False,damage=0)
        self.wolves+=1
        for prop, value in [('token',1),('color',8),('subtype_Wolf',1)]:
            self.emit('assert_matching_permanents', player=self.owner,index=self.rows[name]['csvid'],property=prop,value=value)

    def hunt_front(self):
        self.life[self.owner]+=2
        self.emit('assert_life', player=self.owner, value=self.life[self.owner])
        self.make_wolf()

    def cast_source(self):
        fee=list(self.shape['price'])
        denied=self.mode=='short_cast'
        if denied:
            fee[next(i for i,n in enumerate(fee) if n)]-=1
        self.fund(self.owner,fee)
        self.emit('cast',ref='source',value=int(not denied))
        self.emit('assert_stack',value=int(not denied))
        if not denied:
            self.pools[self.owner]=[0]*6
            self.observe_mana()
            self.emit('resolve')
            self.cards['source'].update(zone='battlefield',tapped=False,damage=0)
            self.sick='Haste' not in self.shape['front']['keywords']
            self.history[self.owner]+=1
            if self.model=='huntmaster':
                self.hunt_front()
        self.receipts()

    def damage(self, ref, amount):
        item=self.cards[ref]
        item['damage']+=amount
        if item['damage']>=self.stats(ref)[1]:
            item['zone']='graveyard'

    def end_rider(self):
        if self.model=='mayor' and self.is_back and self.current==self.owner:
            self.make_wolf()

    def upkeep_intents(self, becoming_back):
        if not becoming_back:
            return
        if self.model=='deserter' and 'artifact' in self.cards:
            if self.mode=='decline':
                self.emit('choose_cancel_target',player=self.owner)
            else:
                self.emit('choose_target',player=self.owner,ref='artifact')
        if self.model=='huntmaster':
            if self.mode=='walker':
                self.emit('choose_target',player=self.owner,ref='walker')
            else:
                self.emit('choose_player',player=self.owner,value=self.foe)
            if 'blocker' in self.cards:
                if self.mode in ('decline_creature','trample'):
                    self.emit('choose_cancel_target',player=self.owner)
                else:
                    self.emit('choose_target',player=self.owner,ref='blocker')

    def apply_transform(self):
        if self.is_back:
            if self.model=='deserter' and self.mode in ('enemy_artifact','own_artifact'):
                victim=self.cards['artifact']
                victim['zone']='graveyard'
                self.life[victim['player']]-=3
            if self.model=='huntmaster':
                if self.mode=='walker':
                    self.loyalty-=2
                else:
                    self.life[self.foe]-=2
                if 'blocker' in self.cards and self.mode not in ('decline_creature','trample'):
                    self.damage('blocker',2)
        elif self.model=='huntmaster':
            self.hunt_front()

    def visit(self, *, capture=False):
        if any(map(any,self.pools)):
            raise ValueError('real turn boundary requires empty mana')
        self.emit('advance_turn')
        self.end_rider()
        self.current=1-self.current
        for item in self.cards.values():
            item['damage']=0
            if item['zone']=='battlefield' and item['player']==self.current:
                item['tapped']=False
        if self.current==self.owner:
            self.sick=False
        self.emit('untap_step',player=self.current)
        self.receipts()
        if capture:
            self.snapshot=len(self.steps)
        becoming_back=not self.is_back and sum(self.history)==0
        changing=becoming_back or (self.is_back and max(self.history)>=2)
        self.upkeep_intents(becoming_back)
        self.emit('upkeep_step',player=self.current)
        if changing:
            self.is_back=not self.is_back
            self.apply_transform()
        self.history=[0,0]
        self.receipts()
        self.emit('turn',player=self.current,value=0)

    def draw_cards(self, amount):
        available=[item for item in self.cards.values() if item['player']==self.owner and item['zone']=='library']
        if len(available)<amount:
            raise ValueError('bounded known draw witnesses required')
        for item in available[:amount]:
            item['zone']='hand'

    def announce_fog(self, who, *, countered=False):
        ref=self.fog(who,pending=True)
        self.emit('announce_spell',ref=ref)
        if self.model=='magehunter' and self.is_back and who==self.foe:
            self.life[self.foe]-=2
            self.emit('assert_life',player=self.foe,value=self.life[self.foe])
        if self.model=='hermit' and who==self.foe and self.current==self.owner:
            self.draw_cards(2 if self.is_back else 1)
        if countered:
            self.spell('counter',response=True)
            self.emit('resolve_response')
            self.cards['counter']['zone']='graveyard'
        else:
            self.emit('resolve')
        self.cards[ref]['zone']='graveyard'
        self.receipts()

    def combat(self, *, attack='source', block=None, fog=False, trample=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attack,value=1)
        self.cards[attack]['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if block:
            self.emit('block',ref=block,other=attack,value=1)
        self.emit('advance_combat')
        amounts={ref:self.stats(ref)[0] for ref in (attack,block) if ref}
        if self.model=='kessig' and block:
            target=block if attack=='source' else attack
            if not self.is_back:
                self.fog(self.current)
                fog = True
            self.damage(target, 2 if self.is_back else 1)
            if fog:
                self.receipts()
        # The bounded declaration callback enqueues the Werewolf packet.
        # Without a response, combat_damage processes it before normal damage.
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if not fog:
            if not block:
                damage=amounts[attack]
                self.life[1-self.current]-=damage
                if self.model=='elder' and self.is_back:
                    self.draw_cards(1)
            elif self.cards[attack]['zone']==self.cards[block]['zone']=='battlefield':
                spill=max(0,amounts[attack]-self.stats(block)[1]) if trample else 0
                self.damage(block,amounts[attack]-spill)
                self.damage(attack,amounts[block])
                self.life[1-self.current]-=spill
        self.receipts()

    def cast_permanent(self, ref, *, transform_entrant=False):
        from recipes import cost_from_oracle

        item=self.cards[ref]
        who=item['player']
        record=self.oracle[item['name']]
        self.fund(who,cost_from_oracle(record.get('selected_face',record)))
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=1)
        self.pools[who]=[0]*6
        self.observe_mana()
        self.history[who]+=1
        if transform_entrant and self.owner==0:
            self.emit('choose_trigger_source',player=self.owner,ref='source')
        self.emit('resolve')
        item['zone']='battlefield'
        if transform_entrant:
            self.additional_face=EXTRA_PAIR[1]
        if ref=='walker':
            self.loyalty=3
        self.receipts()

    def run(self):
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            self.fund(who,[0]*6)
        self.cast_source()
        mode, model = self.mode, self.model
        if mode in ('short_cast','entry'):
            return
        if mode=='haste':
            self.combat()
            return
        if mode in ('front','front_foe'):
            self.announce_fog(self.foe)
            return
        self.visit()
        if mode=='front_blocks':
            self.combat(attack='enemy_attacker',block='source')
            return
        if mode in ('front_hit','front_blocked'):
            self.announce_fog(self.foe)
            self.visit()
            self.combat(block='blocker' if mode=='front_blocked' else None)
            return
        if mode=='walker':
            self.cast_permanent('walker')
            self.visit()
        self.visit()
        if mode in ('split_stays','replay','flip_back_pair','revert_lords'):
            self.announce_fog(self.owner)
            self.announce_fog(self.foe if mode=='split_stays' else self.owner)
            self.visit(capture=mode=='replay')
        elif mode in ('back_hit','blocked','trample','back_blocked','unblocked','no_hit'):
            if mode=='no_hit':
                self.announce_fog(self.foe)
            self.combat(block='blocker' if mode in ('blocked','trample','back_blocked') else None,
                        fog=mode=='no_hit',trample=mode=='trample')
        elif mode=='back_blocks':
            self.visit()
            self.combat(attack='enemy_attacker',block='source')
        elif mode in ('back_owner','own_spell'):
            self.announce_fog(self.owner)
        elif mode in ('back','back_enemy','countered','not_your_turn','offturn_enemy'):
            if mode in ('not_your_turn','offturn_enemy'):
                self.visit()
            self.announce_fog(self.foe,countered=mode=='countered')
        elif model=='mayor':
            if mode=='buffs':
                self.cast_permanent('entrant')
            if mode=='own_end_again':
                self.visit()
                self.visit()
            self.emit('cleanup')
            self.end_rider()
            self.receipts()
        elif model=='bandit':
            if mode=='other_wolf':
                self.visit()
            self.cast_permanent('entrant',transform_entrant=mode=='own_wolf')


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or len(set(selected))!=len(selected) or set(selected)-set(variants(shape)):
        raise ValueError('only explicitly selected physical front cases may expand')
    cases=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1)
        world=Riders(name,shape,mode,int(owner),rows,oracle)
        world.run()
        steps=world.steps
        if world.snapshot is not None:
            cut=world.snapshot
            suffix=copy.deepcopy(steps[cut:])
            steps=steps[:cut]+[step('capture')]+suffix+[step('record_state'),step('restore')]+copy.deepcopy(suffix)+[step('assert_recorded_state')]
        gaps=['The driver resolves upkeep and transform targets atomically rather than exposing priority inside the trigger.']
        if shape['model']=='bandit':
            gaps.append('Declining the optional Vildin entry uses the actual human choice. Its production AI resolves the trigger automatically.')
        cases.append(dict(name=f'WerewolfTriggeredRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=123,setup=world.setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
