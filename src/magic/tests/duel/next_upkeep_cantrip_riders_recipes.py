"""Immediate targeted spell effects and the next turn's real upkeep draw."""

import copy
import json
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'next_upkeep_cantrip_riders'
PROFILE = Path(__file__).parent / 'data/next_upkeep_cantrip_riders_wave8_profiles.json'
DRAW = "Draw a card at the beginning of the next turn's upkeep."
MODES = {
    "Look at the top three cards of target player's library, then put them back in any order. You may have that player shuffle.":
        ('order','portent','Sorcery',[]),
    "Target creature can't be regenerated this turn.": ('no_regen','gravebind','Instant',[]),
    'Exile target card from a graveyard.': ('exile','headstone','Instant',[]),
    "Reveal the top card of target opponent's library. If it's a land, you gain 1 life. Then that player shuffles.":
        ('reveal','prophecy','Sorcery',[]),
    'Target player mills a card.': ('mill','ray_of_erasure','Instant',['Mill']),
    'Touch of Death deals 1 damage to target player or planeswalker. You gain 1 life.': ('drain','touch_of_death','Sorcery',[]),
    'Target creature gains flying until end of turn.': ('fly','updraft','Instant',[]),
}
COMMON = ('effect','short','countered','replay','repeat')
EXTRA = {
    'order': ('self_order','shuffle','short_target','empty_target'),
    'no_regen': ('prevent','unrelated','expiry','lost_target'),
    'exile': ('own_grave','single_grave','no_grave','lost_target'),
    'reveal': ('nonland','empty_target'),
    'mill': ('own_library','empty_target'),
    'drain': ('self','walker'),
    'fly': ('attack','expiry','lost_target'),
}
WITNESSES = {'Forest','Island','Mountain','Swamp','Plains','Counterspell','Drudge Skeletons','Grizzly Bears',
             'Hill Giant','Cremate','Unsummon','Jace Beleren'}
DUMMIES = {'Grizzly Bears','Hill Giant'}
LIBRARY_OWNER = ('Forest','Island','Mountain','Plains')
LIBRARY_FOE = ('Swamp','Island','Plains','Mountain')


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or len(lines) != 2
            or lines[1] != DRAW or lines[0] not in MODES):
        raise ValueError('whole next-turn upkeep cantrip required')
    mode,_,kind,keys = MODES[lines[0]]
    if kind != record['type_line'] or keys != record['keywords']:
        raise ValueError('printed spell type or keyword differs from the whole modeled spell')
    return dict(mode=mode, instant=kind=='Instant', cost=cost_from_oracle(record), keywords=[])


def handler(record):
    return 'manalinkeh_card_' + MODES[record['oracle_text'].splitlines()[0]][1]


def variants(params):
    extra = ('offturn',) if params['instant'] else ()
    return [f'{scene}_{seat}' for scene in (*COMMON,*extra,*EXTRA[params['mode']]) for seat in (0,1)
            if params['mode']!='no_regen' or seat==0 or scene=='short']


def spec(declaration, record):
    p = classify(record)
    pin = profiles()['sources'].get(record['name'])
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not pin or pin['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'),sort_keys=True) != json.dumps(p,sort_keys=True)):
        raise ValueError('exact pinned delayed cantrip and initial effect required')
    if (type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected)) != len(selected) or not set(selected) <= set(variants(p))):
        raise ValueError('distinct approved next-upkeep cantrip variants required')
    return dict(p,family=FAMILY,effect=FAMILY)


def validate_witnesses(oracle, registry):
    data = profiles()
    if data.get('version') != 1 or set(data['witnesses']) != WITNESSES:
        raise ValueError('all independent delay/effect witnesses require pins')
    for group in ('sources','witnesses'):
        for name,pin in data[group].items():
            record,row = oracle.get(name,{}), registry.get(name,{})
            vanilla = group == 'witnesses' and name in DUMMIES and pin.get('reviewed_native_characteristics')
            if (pin['oracle'] != record or pin['registry'] != row or row.get('kind') != 'card'
                    or record['layout'] != 'normal' or row.get('handler') in (None,'NULL') or row['handler'] == 'card_dummy' and not vanilla):
                raise ValueError(f'changed delayed cantrip source or witness: {name}')
            if group == 'sources':
                classify(record)
                if row['handler'] != handler(record):
                    raise ValueError(f'changed delayed cantrip source callback: {name}')


class _World(_SpellWorld):
    def __init__(self,name,p,owner,registry,oracle,scene):
        super().__init__(name,p,owner,registry,oracle)
        self.scene = scene
        self.shuffles = [0,0]
        self.unordered = set()
        self.loyalty = 0
        self.grave_choice = []

    def seed_library(self,seat,contents):
        for index,card in enumerate(contents):
            self.add(card,f'lib_{seat}_{index}',seat,'library')

    def receipts(self):
        start = len(self.steps)
        super().receipts()
        if self.unordered:
            self.steps[start:] = [step for step in self.steps[start:]
                if not (step['op']=='assert_zone_card' and step['zone']=='library' and step['player'] in self.unordered)]
        for seat in (0,1):
            self.emit('assert_shuffle_count',player=seat,value=self.shuffles[seat])
        for seat in sorted(self.unordered):
            deck = self.zones['library'][seat]
            for name in sorted(set(deck)):
                self.emit('assert_zone_multiplicity',player=seat,zone='library',index=self.rows[name]['csvid'],value=deck.count(name))
        if self.loyalty:
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.loyalty)

    def draw(self, seat, amount):
        for _ in range(amount):
            self.zones['hand'][seat].append(self.zones['library'][seat].pop(0))

    def next_upkeep(self, receipts=0):
        if any(map(any,self.mana)):
            raise ValueError('real upkeep requires spent mana')
        self.emit('advance_turn')
        self.expire()
        self.current = 1-self.current
        self.emit('untap_step',player=self.current)
        for creature in self.entities.values():
            if creature['zone']=='battlefield' and creature['player']==self.current:
                creature.update(tapped=False,sick=False)
        # No draw on the new turn or during untap.
        self.receipts()
        self.emit('upkeep_step',player=self.current)
        self.draw(self.owner,receipts)
        self.receipts()
        self.emit('turn',player=self.current,value=0)

    def setup_walker(self):
        self.next_upkeep()
        self.cast('walker')
        self.finish('walker','battlefield')
        self.loyalty = 3
        self.receipts()
        self.next_upkeep()

    def target(self):
        scene,mode = self.scene,self.p['mode']
        if mode in ('no_regen','fly'):
            return 'recipient',None
        if mode=='drain' and scene=='walker':
            return 'walker',None
        if mode=='exile':
            return None,None
        return None,self.owner if scene in ('self_order','own_library','self') else self.foe

    def select_grave(self,actor,target,card):
        if all(self.zones['graveyard']):
            self.emit('choose_player',player=actor,value=target)
        self.emit('choose_zone_card',player=actor,value=self.rows[card]['csvid'])

    def pay(self,ref,*,allowed=True,limit=None,spell_target=None,player_target=None,grave_target=None,response=False):
        from recipes import cost_from_oracle

        name,actor,_ = self.cards[ref]
        fee = cost_from_oracle(self.o[name])
        budget = list(fee if limit is None else limit)
        self.pool(actor,budget)
        self.emit('refresh')
        if allowed and spell_target:
            self.emit('choose_target',ref=spell_target,player=actor)
        elif allowed and player_target is not None:
            self.emit('choose_player',player=actor,value=player_target)
        elif allowed and grave_target:
            self.select_grave(actor,*grave_target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        self.emit('assert_stack',value=(2 if response else 1) if allowed else 0)
        self.emit('assert_unused_choices',value=0)
        self.observe(ref,'in_hand',not allowed)
        if allowed:
            self.mana[actor] = [a-b for a,b in zip(budget,fee)]
        for seat in (0,1):
            for color,amount in enumerate(self.mana[seat]):
                self.emit('assert_mana',player=seat,color=color,value=amount)
        if allowed:
            # Avoid issuing a dependent resolve or announce on an absent card.
            self.emit('require_card',ref=ref,property='in_hand',value=0)

    def reorder(self, target):
        deck = self.zones['library'][target]
        n = min(3,len(deck))
        if n==1:
            self.emit('choose_zone',player=self.owner,value=-1)
        elif n>1 and self.owner==0:
            if self.scene!='shuffle':
                card = deck.pop(n-1)
                self.emit('choose_zone_card',player=self.owner,value=self.rows[card]['csvid'])
                deck.insert(0,card)
            self.emit('choose_zone',player=self.owner,value=-1)
        self.emit('present_ai_dialogs',value=1)
        self.emit('choose_dialog',player=self.owner,value=0 if self.scene=='shuffle' else 1)
        if self.scene=='shuffle':
            self.shuffles[target] += 1
            self.unordered.add(target)

    def prophecy(self,target):
        deck = self.zones['library'][target]
        if deck:
            self.emit('present_ai_dialogs',value=1)
            if self.owner==0:
                self.emit('choose_dialog',player=0,value=0)
            if 'Land' in self.o[deck[0]]['type_line'].split():
                self.life[self.owner] += 1
        self.shuffles[target] += 1
        self.unordered.add(target)

    def immediate(self,ref,target,player,grave):
        mode = self.p['mode']
        if mode=='order':
            self.reorder(player)
        elif mode=='reveal':
            self.prophecy(player)
        elif mode=='exile':
            seat,card = grave
            self.zones['graveyard'][seat].remove(card)
            self.zones['exile'][seat].append(card)
        elif mode=='mill' and self.zones['library'][player]:
            self.zones['graveyard'][player].append(self.zones['library'][player].pop(0))
        elif mode=='drain':
            self.life[self.owner] += 1
            if target:
                self.loyalty -= 1
            else:
                self.life[player] -= 1
        self.finish(ref,'graveyard')
        if mode in ('order','reveal'):
            self.emit('present_ai_dialogs',value=0)
        self.receipts()

    def response(self,ref,target,grave):
        self.emit('announce_spell',ref=ref)
        if self.scene=='countered':
            self.pay('reply',response=True)
        elif grave is not None:
            self.pay('reply',grave_target=grave,response=True)
            seat,card = grave
            self.zones['graveyard'][seat].remove(card)
            self.zones['exile'][seat].append(card)
            self.draw(self.foe,1)
        else:
            self.pay('reply',spell_target=target,response=True)
            self.move(target,'hand')
        self.finish('reply','graveyard',response=True)
        self.move(ref,'graveyard')
        self.receipts()

    def regenerate(self,target,saves):
        from recipes import COLORS

        victim = self.entities[target]
        seat = victim['player']
        fee = [0]*6
        fee[COLORS['B']] = 1
        self.pool(seat,fee)
        self.emit('refresh')
        self.emit('choose_regeneration',ref=target)
        self.emit('destroy',ref=target)
        self.emit('assert_unused_choices',value=int(not saves))
        if saves:
            self.mana[seat] = [0]*6
            victim.update(tapped=True,damage=0,attacking=False)
        else:
            self.move(target,'graveyard')
        self.receipts()

    def flying_combat(self,flying):
        self.emit('begin_attackers')
        self.emit('attack',ref='recipient',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='recipient',value=int(not flying))
        self.observe('blocker','blocking_count',not flying)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        if flying:
            self.entities['recipient']['tapped'] = True
            self.life[self.foe] -= 2
        else:
            self.entities['blocker']['damage'] = 2
            self.move('recipient','graveyard')
        self.receipts()


def fixture(w):
    scene,mode,owner,foe = w.scene,w.p['mode'],w.owner,w.foe
    w.add(w.name,'source',owner,'hand')
    if scene=='repeat':
        w.add(w.name,'second',owner,'hand')
    if scene=='countered':
        w.add('Counterspell','reply',foe,'hand')
    if scene=='lost_target':
        w.add('Cremate' if mode=='exile' else 'Unsummon','reply',foe,'hand')
    own_deck, foe_deck = LIBRARY_OWNER, LIBRARY_FOE
    if mode=='order':
        if owner==1:
            if scene=='self_order':
                own_deck = ('Island','Island','Island','Forest')
            else:
                foe_deck = ('Island','Island','Island','Swamp')
        if scene=='short_target':
            foe_deck = ('Swamp',)
    if mode=='reveal':
        if scene=='nonland':
            foe_deck = ('Grizzly Bears','Island','Plains')
        elif scene=='repeat':
            foe_deck = ('Swamp',)
    if scene=='empty_target':
        foe_deck = ()
    w.seed_library(owner,own_deck)
    w.seed_library(foe,foe_deck)
    if mode=='exile' and scene!='no_grave':
        if scene not in ('single_grave','lost_target'):
            w.add('Mountain','own_grave',owner,'graveyard')
        w.add('Grizzly Bears','foe_grave',foe,'graveyard')
        if scene=='repeat':
            w.add('Hill Giant','other_grave',foe,'graveyard')
        w.grave_choice = [(owner,'Mountain')] if scene=='own_grave' else [(foe,'Grizzly Bears')]
        if scene=='repeat':
            w.grave_choice.append((foe,'Hill Giant'))
    if mode=='no_regen':
        w.add('Drudge Skeletons','recipient',foe)
        if scene=='unrelated':
            w.add('Drudge Skeletons','unrelated',foe)
    if mode=='fly':
        w.add('Grizzly Bears','recipient',owner)
        if scene in ('attack','expiry'):
            w.add('Hill Giant','blocker',foe)
    if scene=='walker':
        w.add('Jace Beleren','walker',foe,'hand')


def run_spell(w):
    mode,scene = w.p['mode'],w.scene
    target, player = w.target()
    if scene=='short':
        fee = list(w.p['cost'])
        fee[next(i for i,v in enumerate(fee) if v)] -= 1
        w.pay('source',allowed=False,limit=fee)
        w.receipts()
        return 0
    if scene=='no_grave':
        w.pay('source',allowed=False)
        w.receipts()
        # A denied cast spends nothing. This fixture terminates before upkeep.
        return None
    due = 0
    for index,ref in enumerate(('source','second') if scene=='repeat' else ('source',)):
        grave = w.grave_choice[index] if w.grave_choice else None
        # Prophecy always targets the sole opponent; the cast callback selects it.
        w.pay(ref,spell_target=target,player_target=None if mode=='reveal' else player,grave_target=grave)
        if scene in ('countered','lost_target'):
            w.response(ref,target,grave)
        else:
            w.immediate(ref,target,player,grave)
            due += 1
    return due


def build_variant(name,p,scene,owner,registry,oracle):
    from recipes import step

    w = _World(name,p,owner,registry,oracle,scene)
    fixture(w)
    w.emit('turn',player=owner,value=0)
    for seat in (0,1):
        w.pool(seat,[0]*6)
    w.receipts()
    if scene=='offturn':
        w.next_upkeep()
    if scene=='walker':
        w.setup_walker()
    if scene=='replay':
        w.emit('capture')
    cut = len(w.steps)
    due = run_spell(w)
    if scene=='short':
        # A failed spell leaves its actual funding in the pool.
        return w.setup,w.steps
    if due is None:
        return w.setup,w.steps
    if scene=='prevent':
        w.regenerate('recipient',saves=False)
        return w.setup,w.steps
    if scene=='unrelated':
        w.regenerate('unrelated',saves=True)
    if scene in ('attack','expiry') and p['mode']=='fly':
        w.flying_combat(flying=True)
    w.next_upkeep(due)
    if scene=='expiry' and p['mode']=='no_regen':
        w.regenerate('recipient',saves=True)
    w.next_upkeep()
    if scene=='expiry' and p['mode']=='fly':
        w.flying_combat(flying=False)
    if scene=='replay':
        tail=copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'),step('ai_roundtrip'),step('restore'),*tail,step('assert_recorded_state')])
    return w.setup,w.steps


def expand(name,csvid,declaration,p,selected,registry,oracle):
    row,record=registry[name],oracle[name]
    pinned=profiles()['sources'].get(name)
    if not pinned or pinned['registry']!=row or row['csvid']!=csvid or row['handler']!=handler(record):
        raise ValueError(f'unpinned delayed cantrip: {name}')
    cases=[]
    for variant in selected:
        scene,owner=variant.rsplit('_',1)
        setup,steps=build_variant(name,p,scene,int(owner),registry,oracle)
        cases.append(dict(name=f'NextUpkeepCantripRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],
                          untested_components=['multiplayer','skipped or additional upkeeps','AI library reorder preference'])))
    return cases
