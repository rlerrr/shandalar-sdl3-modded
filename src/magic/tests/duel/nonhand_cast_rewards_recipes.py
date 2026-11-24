"""Whole-card cast rewards with real cast-zone and historic distinctions."""

from collections import Counter
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from global_attack_tax_recipes import Board


FAMILY = 'nonhand_cast_rewards'
PROFILE = Path(__file__).parent / 'data/nonhand_cast_rewards_wave8_profiles.json'
FLASH = 'Think Twice'
ARTIFACT = 'Mind Stone'
LEGEND = 'Isamaru, Hound of Konda'
SAGA = 'The Flame of Keld'
SMALL = 'Elvish Visionary'
BOUNDARY = 'Centaur Courser'
BIG = 'Hill Giant'
DECK = ('Forest', 'Island', 'Mountain', 'Plains', 'Forest', 'Island')
WITNESSES = set(DECK) | {FLASH, ARTIFACT, LEGEND, SAGA, SMALL, BOUNDARY, BIG, 'Grizzly Bears',
                         'Giant Spider', 'Jace Beleren', 'Act on Impulse', 'Counterspell', 'Dovescape',
                         'Bird', 'Visions of Beyond'}
STATIC_WITNESSES = {'Centaur Courser', 'Grizzly Bears', 'Giant Spider', 'Hill Giant', 'Bird'}
GRAVE = ('flashback', 'offturn', 'other_flashback', 'hand', 'exile', 'underpaid', 'countered', 'replay')
HISTORIC = ('artifact', 'legendary', 'saga', 'exile_artifact', 'countered', 'opponent', 'nonhistoric',
            'flying', 'reach', 'replay')
MODELS = {
    ('Enchantment', '{2}{R}', (),
     'Whenever you cast a spell from your graveyard, this enchantment deals 2 damage to any target.'): 'damage',
    ('Enchantment', '{2}{U}', (),
     'Whenever you cast a spell from your graveyard, draw a card.'): 'draw',
    ('Enchantment', '{2}{R}', (),
     'Whenever you cast an instant or sorcery spell from your hand, you may cast target card with the same name as that spell from your graveyard. (You still pay its costs.)'): 'matching',
    ('Legendary Creature — Bird Cleric', '{3}{W}', ('Flying',),
     'Flying\nWhenever you cast a historic spell, return target creature card with mana value 3 or less from your graveyard to the battlefield. (Artifacts, legendaries, and Sagas are historic.)'): 'return',
    ('Creature — Bird', '{U}', ('Scry', 'Flying'),
     'Flying\nWhenever you cast a historic spell, scry 1. (Artifacts, legendaries, and Sagas are historic. To scry 1, look at the top card of your library, then you may put that card on the bottom.)'): 'scry',
}
MODES = {'damage': GRAVE+('creature', 'walker'), 'draw': GRAVE,
         'matching': ('paid', 'underpaid', 'decline', 'other_name', 'grave', 'exile', 'opponent', 'artifact', 'replay'),
         'return': HISTORIC+('boundary', 'no_eligible', 'return_etb', 'empty'),
         'scry': HISTORIC+('keep', 'next_turn', 'empty')}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    key = record['type_line'], record['mana_cost'], tuple(record['keywords']), record['oracle_text']
    if record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces') or key not in MODELS:
        raise ValueError('normal primary with exact cast reward required')
    model = MODELS[key]
    if model in ('return', 'scry'):
        stat = '2' if model=='return' else '1'
        if (record.get('power'),record.get('toughness'))!=(stat,stat):
            raise ValueError('printed Flying creature stats drift')
    return dict(model=model, cost=cost_from_oracle(record))


def variants(params):
    return [f'{name}_{seat}' for seat in (0,1) for name in MODES[params['model']]]


def spec(declaration, record):
    params = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profile()['sources'].get(record['name'], {}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)):
        raise ValueError('pinned whole Oracle and cast reward model required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(params))):
        raise ValueError('distinct calibrated variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses'])!=WITNESSES or len(pins['sources'])!=len(MODES):
        raise ValueError('complete cast reward action witnesses required')
    for name, entry in (pins['sources']|pins['witnesses']).items():
        row=rows[name]
        valid_kind = ('token','card') if name=='Bird' else ('card',)
        if row['kind'] not in valid_kind or not exact(row,entry['registry']) or not exact(oracle[name],entry['oracle']):
            raise ValueError(f'Oracle or registry drift in {name}')
        if name not in STATIC_WITNESSES and row['handler'] in ('card_dummy','0','NULL',None):
            raise ValueError(f'live action requires implemented witness {name}')
    if oracle[FLASH]['oracle_text']!=('Draw a card.\nFlashback {2}{U} (You may cast this card from your graveyard for its flashback cost. Then exile it.)'):
        raise ValueError('targetless fixed flashback witness required')
    if [oracle[card]['cmc'] for card in (SMALL,BOUNDARY,BIG)]!=[2,3,4]:
        raise ValueError('exact neighboring mana values required')


class CastBoard(Board):
    def __init__(self,name,params,variant,rows,oracle):
        self.mode, seat = variant.rsplit('_',1)
        self.owner, self.rows, self.oracle = int(seat),rows,oracle
        self.enemy = 1-self.owner
        self.name,self.model = name,params['model']
        self.active = self.enemy if self.mode in ('offturn','other_flashback','opponent','walker') else self.owner
        self.actor = self.enemy if self.mode in ('other_flashback','opponent') else self.owner
        self.setup,self.steps,self.entities = [],[],{}
        self.zones = {zone:[[],[]] for zone in ('hand','battlefield','graveyard','library','exile')}
        self.mana,self.life = [[0]*6,[0]*6],[20,20]
        self.replay = None
        self.loyalty = None
        self.add(name,'source',self.owner)
        self.parent = FLASH
        self.grave_target = BOUNDARY if self.mode in ('boundary','saga') else SMALL
        self.exile_parent = self.mode in ('exile','exile_artifact')
        self.grave_parent = self.mode in ('flashback','offturn','other_flashback','underpaid','countered','replay','creature','walker')
        if self.model=='matching':
            self.grave_parent = self.mode=='grave'
            self.parent = ARTIFACT if self.mode=='artifact' else FLASH
        if self.historic:
            self.grave_parent = False
            self.parent = {'legendary':LEGEND,'saga':SAGA,'nonhistoric':FLASH}.get(self.mode,ARTIFACT)
        flying = self.mode in ('flying','reach')
        if self.exile_parent:
            self.add('Act on Impulse','impulse',self.owner,'hand')
        elif not flying:
            self.add(self.parent,'parent',self.actor,'graveyard' if self.grave_parent else 'hand')
        if self.mode=='next_turn':
            self.add(FLASH,'later',self.owner,'hand')
        if self.mode=='countered':
            if self.grave_parent:
                self.add('Dovescape','counter_effect',self.enemy)
            else:
                self.add('Counterspell','counter',self.enemy,'hand')
        if self.mode=='creature':
            self.add('Grizzly Bears','recipient',self.enemy)
        if self.mode=='walker':
            self.add('Jace Beleren','walker',self.enemy,'hand')
        if flying:
            self.add('Giant Spider' if self.mode=='reach' else 'Grizzly Bears','blocker',self.enemy)
        if self.model=='matching':
            match = 'Visions of Beyond' if self.mode=='other_name' else self.parent
            self.add(match,'candidate',self.owner,'graveyard')
        if self.model=='return' and self.mode!='empty' and not flying:
            self.add(BIG,'over',self.owner,'graveyard')
            self.add(FLASH,'type_wrong',self.owner,'graveyard')
            self.add(self.grave_target,'off_owner',self.enemy,'graveyard')
            if self.mode!='no_eligible':
                self.add(self.grave_target,'returnable',self.owner,'graveyard')
        for who in (0,1):
            deck=list(DECK)
            if self.model=='scry' and who==self.owner:
                deck=[] if self.mode=='empty' else [BIG,*deck]
            if self.exile_parent and who==self.owner:
                deck=[self.parent,'Mountain','Plains',*deck]
            for index,card in enumerate(deck):
                self.add(card,f'library_{who}_{index}',who,'library')
        self.names = {card for zone in self.zones.values() for pile in zone for card in pile}|{'Bird'}

    @property
    def historic(self):
        return self.model in ('return','scry')

    def receipt(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone, players in self.zones.items():
                cards = players[who]
                self.emit('assert_zone',player=who,zone=zone,value=len(cards))
                if zone=='library':
                    for index,card in enumerate(cards):
                        self.emit('assert_zone_card',player=who,index=index,zone=zone,value=self.rows[card]['csvid'])
                else:
                    counts=Counter(cards)
                    for card in sorted(self.names):
                        self.emit('assert_zone_multiplicity',player=who,index=self.rows[card]['csvid'],zone=zone,value=counts[card])
        if self.loyalty is not None:
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.loyalty)

    def fee(self,name):
        from recipes import cost_from_oracle

        return cost_from_oracle(self.oracle[name])

    def spend(self,who,fee):
        self.mana[who] = [have-due for have,due in zip(self.mana[who],fee)]
        assert min(self.mana[who])>=0

    def move_card(self,who,name,before,after):
        self.zones[before][who].remove(name)
        self.zones[after][who].append(name)

    def draw(self,who):
        self.zones['hand'][who].append(self.zones['library'][who].pop(0))

    def start(self,ref,*,response=False):
        who, name = self.entities[ref]['player'],self.entities[ref]['name']
        self.emit('cast_response' if response else 'cast',ref=ref,value=1)
        self.emit('assert_stack',value=2 if response else 1)
        self.observe(ref,'in_hand',0)
        self.spend(who,self.fee(name))
        self.pools()

    def decide_reward(self):
        if self.model=='damage':
            target={'creature':'recipient','walker':'walker'}.get(self.mode)
            if target:
                self.emit('choose_target',player=self.owner,ref=target)
            else:
                self.emit('choose_player',player=self.owner,value=self.enemy)
            return
        if self.model not in ('return','scry','matching'):
            return
        if self.model=='scry':
            self.emit('record_library',player=self.owner)
        if self.owner==0:
            # The callbacks use RESOLVE_TRIGGER_AI or its equivalent for HUMAN.
            self.emit('choose_trigger',player=self.owner,ref='source')
        if self.model=='return' and self.mode not in ('empty','no_eligible'):
            self.emit('observe_search_filters',value=1)
            self.emit('choose_zone_card',player=self.owner,value=self.rows[self.grave_target]['csvid'])
        if self.model=='scry' and self.zones['library'][self.owner]:
            if self.mode=='keep':
                self.emit('choose_zone',player=self.owner,value=-1)
            else:
                self.emit('choose_zone_card',player=self.owner,value=self.rows[self.zones['library'][self.owner][0]]['csvid'])
        if self.model=='matching':
            self.emit('observe_search_filters',value=1)
            if self.mode=='decline':
                self.emit('choose_zone',player=self.owner,value=-1)
            else:
                self.emit('choose_zone_card',player=self.owner,value=self.rows[FLASH]['csvid'])

    def observe_reward(self):
        self.emit('assert_unused_choices',value=0)
        if self.model=='damage':
            if self.mode=='creature':
                self.move('recipient','graveyard')
            elif self.mode=='walker':
                self.loyalty-=2
            else:
                self.life[self.enemy]-=2
        elif self.model=='draw':
            self.draw(self.owner)
        elif self.model=='matching':
            if self.mode not in ('underpaid','decline'):
                self.spend(self.owner,self.fee(FLASH))
                self.draw(self.owner)
        elif self.model=='return' and self.mode not in ('empty','no_eligible'):
            self.emit('assert_offered_legal_card',index=self.rows[self.grave_target]['csvid'],value=1)
            for illegal in (BIG, FLASH):
                self.emit('assert_offered_legal_card',index=self.rows[illegal]['csvid'],value=0)
            self.move_card(self.owner,self.grave_target,'graveyard','battlefield')
            if self.grave_target==SMALL:
                self.draw(self.owner)
        elif self.model=='scry':
            self.emit('assert_scry_result',player=self.owner,value=1)
            deck=self.zones['library'][self.owner]
            if deck and self.mode!='keep':
                deck.append(deck.pop(0))

    def flashback(self,*,reward):
        fee=[2,0,1,0,0,0]
        short=self.model!='matching' and self.mode=='underpaid'
        self.fund(self.actor,[1,0,1,0,0,0] if short else fee)
        ref='parent'
        self.emit('prepare_graveyard_abilities')
        self.emit('can_cast_graveyard',ref=ref,value=int(not short))
        if reward:
            self.decide_reward()
        if self.mode=='replay':
            self.replay=len(self.steps)
        self.emit('cast_graveyard',ref=ref,value=int(not short))
        if short:
            return
        self.spend(self.actor,fee)
        self.move_card(self.actor,FLASH,'graveyard','exile')
        if reward:
            self.observe_reward()
        if self.mode=='countered':
            self.zones['battlefield'][self.actor]+=['Bird'] * int(self.oracle[FLASH]['cmc'])
            for key,value in [('token',1),('subtype_Bird',1),('power',1),('toughness',1),('color',0x24)]:
                self.emit('assert_matching_permanents',player=self.actor,index=self.rows['Bird']['csvid'],property=key,value=value)
        else:
            self.draw(self.actor)

    def hand_parent(self,ref='parent',*,reward=False,counter=False):
        who,name=self.entities[ref]['player'],self.entities[ref]['name']
        fee=self.fee(name)
        if reward and self.model=='matching' and self.mode!='underpaid':
            fee=[2*amount for amount in fee]
        self.fund(who,fee)
        self.start(ref)
        if self.mode=='replay':
            self.replay=len(self.steps)
        if reward:
            self.decide_reward()
        if counter:
            self.emit('announce_spell',ref=ref)
            self.fund(self.enemy,self.fee('Counterspell'))
            self.start('counter',response=True)
            self.emit('resolve_response')
            self.move('counter','graveyard')
        else:
            self.emit('resolve')
        if reward:
            self.observe_reward()
        spell = self.oracle[name]['type_line'] in ('Instant','Sorcery')
        self.move(ref,'graveyard' if spell or counter else 'battlefield')
        if not counter and name==FLASH:
            self.draw(who)
        if not counter and name==SAGA:
            self.emit('assert_counter',ref=ref,counter_kind='lore',value=1)

    def own_exile(self,*,reward):
        self.fund(self.owner,self.fee('Act on Impulse'))
        self.start('impulse')
        self.emit('resolve')
        self.move('impulse','graveyard')
        top=self.zones['library'][self.owner][:3]
        del self.zones['library'][self.owner][:3]
        self.zones['exile'][self.owner].extend(top)
        self.receipt()
        fee=self.fee(self.parent)
        self.fund(self.owner,fee)
        self.emit('can_play_exiled',player=self.owner,index=self.rows[self.parent]['csvid'],value=1)
        if reward:
            self.decide_reward()
        self.emit('play_exiled',player=self.owner,index=self.rows[self.parent]['csvid'],value=1)
        self.spend(self.owner,fee)
        self.emit('resolve')
        self.move_card(self.owner,self.parent,'exile','graveyard' if self.parent==FLASH else 'battlefield')
        if reward:
            self.observe_reward()
        if self.parent==FLASH:
            self.draw(self.owner)

    def fight(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        blocked=self.mode=='reach'
        self.emit('block',ref='blocker',other='source',value=int(blocked))
        self.emit('advance_combat')
        self.observe('source','blocked',blocked)
        self.emit('combat_damage')
        self.emit('end_combat')
        if blocked:
            self.move('source','graveyard')
        else:
            self.life[self.enemy]-=int(self.oracle[self.name]['power'])

    def actions(self):
        from recipes import step

        self.emit('turn',player=self.active,value=0)
        self.fund(self.owner,[0]*6)
        if self.mode in ('flying','reach'):
            self.fight()
        elif self.historic:
            reward=self.mode not in ('nonhistoric','opponent')
            if self.exile_parent:
                self.own_exile(reward=reward)
            else:
                self.hand_parent(reward=reward,counter=self.mode=='countered')
            if self.mode=='next_turn':
                self.receipt()
                for who in (self.enemy,self.owner):
                    self.emit('advance_turn')
                    self.emit('untap_step',player=who)
                self.emit('turn',player=self.owner,value=0)
                self.hand_parent(ref='later')
        elif self.model=='matching':
            if self.exile_parent:
                self.own_exile(reward=False)
            elif self.grave_parent:
                self.flashback(reward=False)
            else:
                self.hand_parent(reward=self.mode in ('paid','underpaid','decline','replay'))
        elif self.exile_parent:
            self.own_exile(reward=False)
        elif self.grave_parent:
            if self.mode=='walker':
                self.hand_parent(ref='walker')
                self.loyalty=int(self.oracle['Jace Beleren']['loyalty'])
                self.receipt()
                self.emit('advance_turn')
                self.emit('untap_step',player=self.owner)
                self.emit('turn',player=self.owner,value=0)
            self.flashback(reward=self.mode not in ('other_flashback','underpaid'))
        else:
            self.hand_parent()
        self.emit('refresh')
        self.receipt()
        if self.replay is not None:
            repeated=self.steps[self.replay:]
            self.steps[self.replay:]=[step('capture'),*repeated,step('record_state'),step('ai_roundtrip'),
                                     step('restore'),*repeated,step('assert_recorded_state')]


def expand(name,csvid,declaration,params,chosen,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not chosen or set(chosen)-set(variants(params)):
        raise ValueError('unbound cast reward source or variant')
    result=[]
    for variant in chosen:
        board=CastBoard(name,params,variant,rows,oracle)
        board.actions()
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                      oracle_text=declaration['oracle_text'],
                      untested_components=['cast directly from library or command zone',
                                           'copying effects', 'multiplayer and replacement effects'])
        result.append(dict(name=f'NonhandCastRewardRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                           setup=board.setup,steps=board.steps,_recipe=metadata))
    return result
