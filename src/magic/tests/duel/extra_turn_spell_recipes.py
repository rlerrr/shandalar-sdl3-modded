"""Whole extra-turn spells through production selection and end-of-turn dispatch."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact


FAMILY = 'extra_turn_spell'
PROFILE = Path(__file__).parent / 'data/extra_turn_spell_wave8_profiles.json'
EXTRA = 'Take an extra turn after this one.'
LOSS = " At the beginning of that turn's end step, you lose the game."
TARGET_EXTRA = 'Target player takes an extra turn after this one.'
TARGET_DOUBLE = 'Target player takes two extra turns after this one.'
TARGET_BEACON = TARGET_EXTRA+" Shuffle Beacon of Tomorrows into its owner's library."
TARGET_CLAUSES = {TARGET_EXTRA:(1,False),TARGET_DOUBLE:(2,False),TARGET_BEACON:(1,True)}
LANDS = ('Wastes','Swamp','Island','Forest','Mountain','Plains')
# The real Shandalar loss callback records this sentinel instead of subtracting life.
LOSS_SENTINEL = -99
COMMON = ('single','stacked','older_plain','newer_plain','countered','counter_extra_only','unfunded','wrong_color',
          'suppress_foe','suppress_own','suppress_nexus','skip_meditate','normal_skip','opposing_schedule',
          'untap_probe','pending_replay','scheduled_replay')
TARGET_MODES = ('target_self','target_other','target_countered','target_invalidated','target_stacked','target_replay',
                'target_suppressed','target_untap','unfunded','wrong_color')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    text = record.get('oracle_text') if record else None
    if (not record or record.get('layout')!='normal' or 'card_faces' in record or 'selected_face' in record or
            record.get('type_line') not in ('Sorcery','Instant') or record.get('keywords') or
            text not in (EXTRA,EXTRA+LOSS,*TARGET_CLAUSES)):
        raise ValueError('complete fixed extra-turn spell required')
    fee = cost_from_oracle(record)
    if sum(fee)>(10 if text in TARGET_CLAUSES else 5) or not any(fee[1:]):
        raise ValueError('bounded nonzero colored spell cost required')
    model = dict(cast_cost=fee,instant=record['type_line']=='Instant',lose_at_linked_end=text==EXTRA+LOSS)
    if text in TARGET_CLAUSES:
        if model['instant'] or (text==TARGET_BEACON and record['name']!='Beacon of Tomorrows'):
            raise ValueError('whole targeted sorcery and matching printed library return required')
        model['target_turns'],model['shuffle_to_owner'] = TARGET_CLAUSES[text]
    return model


def variants(model):
    if 'target_turns' in model:
        modes = TARGET_MODES+('empty_library',) if model['shuffle_to_owner'] else TARGET_MODES
        return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]
    modes = COMMON
    if model['lose_at_linked_end']:
        modes += ('protected_end',)
    if model['instant']:
        modes += ('offturn','offturn_response')
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes] + ['cancel_0']


def spec(declaration, record):
    model = parameters(record)
    names = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(model,declaration.get('params')) or type(names) is not list or not names or
            any(type(v) is not str for v in names) or len(set(names))!=len(names) or set(names)-set(variants(model))):
        raise ValueError('typed full Oracle extra-turn declaration and explicit variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, registry):
    for group in ('sources','witnesses'):
        for name,pin in pins()[group].items():
            row, record = registry.get(name,{}), oracle.get(name,{})
            if (not exact(pin,{'oracle':record,'registry':row}) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal extra-turn identity changed: {name}')


class Scene:
    def __init__(self, name, model, mode, owner, registry, oracle):
        self.name,self.model,self.mode,self.owner = name,model,mode,owner
        self.registry,self.oracle = registry,oracle
        self.other = 1-owner
        self.active = self.other if mode in ('offturn','offturn_response') else owner
        self.normal_next = 1-self.active
        self.linked_loss = False
        self.skipped = set()
        self.extra = []
        self.steps,self.setup,self.entities,self.stack = [],[],{},[]
        self.life,self.mana = [20,20],[[0]*6 for _ in (0,1)]
        self.shuffles,self.shrouded = [0,0],set()
        self.replay_from = None

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op,**fields))

    def add(self, name, ref, player, zone='hand', tapped=False):
        from recipes import card

        item = card(name,ref,player,zone)
        if tapped:
            item['state'] = ['tapped']
        self.setup.append(item)
        self.entities[ref] = dict(name=name,player=player,zone=zone,tapped=tapped)

    def has(self, name, player=None):
        return any(e['name']==name and e['zone']=='battlefield' and (player is None or e['player']==player)
                   for e in self.entities.values())

    def mana_receipts(self):
        for player,amounts in enumerate(self.mana):
            for color,value in enumerate(amounts):
                self.emit('assert_mana',player=player,color=color,value=value)

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=len(self.stack))
        self.mana_receipts()
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            if self.model.get('shuffle_to_owner'):
                self.emit('assert_shuffle_count',player=player,value=self.shuffles[player])
            if self.stack:
                # Pending legacy spells occupy physical hand slots until resolution.
                continue
            for zone in ('hand','battlefield','graveyard','exile','library'):
                counts = Counter(e['name'] for e in self.entities.values() if e['player']==player and e['zone']==zone)
                self.emit('assert_zone',player=player,zone=zone,value=sum(counts.values()))
                for name,count in counts.items():
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.registry[name]['csvid'],value=count)
            if self.model.get('shuffle_to_owner') and (player!=self.owner or not self.shuffles[player]):
                cards = [e['name'] for e in self.entities.values() if e['player']==player and e['zone']=='library']
                for index,name in enumerate(cards):
                    self.emit('assert_zone_card',player=player,zone='library',index=index,value=self.registry[name]['csvid'])
        if not self.stack:
            for ref,e in self.entities.items():
                if e['zone']=='battlefield':
                    self.emit('assert_card',ref=ref,property='in_play',value=1)
                    self.emit('assert_card',ref=ref,property='tapped',value=int(e['tapped']))

    def cast(self, ref, *, target=None, player_target=None, denied=None, response=False):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['player']
        record = self.oracle[entity['name']]
        assert entity['zone']=='hand' and not any(self.mana[player])
        if (response or player!=self.active) and record['type_line']!='Instant':
            raise ValueError('ordinary sorceries require the controller’s own main phase')
        assert response==bool(self.stack)
        cost = cost_from_oracle(record) if record['mana_cost']!='{0}' else [0]*6
        funding = [0]*6 if denied in ('unfunded','cancel') else [sum(cost),0,0,0,0,0] if denied=='wrong_color' else cost
        self.mana[player] = list(funding)
        for color,amount in enumerate(funding):
            if amount:
                self.emit('set_mana',player=player,color=color,value=amount)
        self.emit('refresh')
        if response:
            self.emit('announce_spell',ref=self.stack[-1])
        # Counterspell binds the current pending spell in its callback.
        # The logical target still identifies the spell that is countered.
        if target is not None and entity['name']!='Counterspell':
            self.emit('choose_target',player=player,ref=target)
        if player_target is not None:
            assert target is None and player_target in (0,1) and denied is None
            self.emit('choose_player',player=player,value=player_target)
        if denied=='cancel':
            assert player==0
            self.emit('choose_cancel_mana',player=player)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(denied is None))
        if denied is None:
            self.mana[player] = [have-price for have,price in zip(funding,cost)]
            self.stack.append(ref)
            entity.update(zone='stack',target=target,player_target=player_target)
            self.emit('assert_card',ref=ref,property='in_hand',value=0)
        self.receipts()

    def resolve(self):
        assert self.stack
        self.emit('resolve_response' if len(self.stack)>1 else 'resolve')
        self.emit('refresh')
        countered = set()
        for ref in reversed(self.stack):
            entity = self.entities[ref]
            name,player = entity['name'],entity['player']
            entity['zone'] = 'graveyard'
            if ref in countered:
                continue
            text = self.oracle[name]['oracle_text']
            if text in (EXTRA,EXTRA+LOSS):
                self.extra.append((player,text==EXTRA+LOSS))
            elif text in TARGET_CLAUSES:
                recipient = entity['player_target']
                assert recipient in (0,1)
                if recipient not in self.shrouded:
                    turns,shuffle = TARGET_CLAUSES[text]
                    self.extra.extend((recipient,False) for _ in range(turns))
                    if shuffle:
                        entity['zone'] = 'library'
                        self.shuffles[player] += 1
            elif name=='Counterspell':
                countered.add(entity['target'])
            elif name=='Gilded Light':
                self.shrouded.add(player)
            elif name=='Meditate':
                library = [e for e in self.entities.values() if e['zone']=='library' and e['player']==player]
                assert len(library)==4
                for card in library:
                    card['zone'] = 'hand'
                self.skipped.add(player)
            elif name=='Naturalize':
                self.entities[entity['target']]['zone'] = 'graveyard'
            elif name=='Darksteel Relic':
                entity['zone'] = 'battlefield'
            else:
                raise ValueError(f'unmodeled resolution for {name}')
        self.stack.clear()
        self.receipts()

    def play_spell(self, ref, **kwargs):
        self.cast(ref,**kwargs)
        self.resolve()

    def advance(self, untap=False):
        assert not self.stack and not any(map(any,self.mana)) and self.active in (0,1)
        self.shrouded.clear()
        if self.linked_loss and not self.has('Platinum Angel',self.active):
            self.life[self.active] = LOSS_SENTINEL
            self.active = -1
            self.emit('advance_scheduled_turn',value=-1)
            self.receipts()
            return False

        while True:
            if self.extra:
                player,loss = self.extra.pop()
                if self.has("Ugin's Nexus") or self.has('Stranglehold',1-player):
                    continue
            else:
                player,loss = self.normal_next,False
                self.normal_next = 1-player
            if player in self.skipped:
                self.skipped.remove(player)
                continue
            break
        self.active,self.linked_loss = player,loss
        self.emit('advance_scheduled_turn',value=player)
        if untap:
            self.emit('untap_step',player=player)
            for entity in self.entities.values():
                if entity['zone']=='battlefield' and entity['player']==player:
                    entity['tapped'] = False
        self.receipts()
        return True

    def watch_turns(self, count):
        for _ in range(count):
            if not self.advance():
                break

    def setup_cards(self):
        mode = self.mode
        self.add(self.name,'source',self.owner)
        if mode=='stacked':
            self.add(self.name,'second',self.owner)
        if mode in ('older_plain','newer_plain','counter_extra_only'):
            self.add('Time Walk','plain',self.owner)
        if mode in ('countered','counter_extra_only'):
            self.add('Counterspell','counter',self.other)
        if mode in ('skip_meditate','normal_skip'):
            player = self.other if mode=='normal_skip' else self.owner
            self.add('Meditate','skip',player)
            for i in range(4):
                self.add('Forest',f'draw_{i}',player,'library')
        if mode in ('suppress_foe','suppress_own'):
            self.add('Stranglehold','suppressor',self.other if mode=='suppress_foe' else self.owner,'battlefield')
        if mode=='suppress_nexus':
            self.add("Ugin's Nexus",'suppressor',self.other,'battlefield')
        if mode=='protected_end':
            self.add('Platinum Angel','protection',self.owner,'battlefield')
            self.add('Naturalize','removal',self.other)
        if mode=='opposing_schedule':
            self.add('Final Fortune','opposition',self.other)
            for player in (0,1):
                self.add('Platinum Angel',f'protection_{player}',player,'battlefield')
        if mode=='offturn_response':
            self.add('Darksteel Relic','parent',self.other)
        if mode=='untap_probe':
            for player in (0,1):
                self.add('Island',f'marker_{player}',player,'battlefield',tapped=True)
        if mode=='cancel':
            for color,amount in enumerate(self.model['cast_cost']):
                for copy in range(amount):
                    self.add(LANDS[color],f'payment_{color}_{copy}',self.owner,'battlefield')
        if mode=='target_countered':
            self.add('Counterspell','counter',self.other)
        if mode=='target_invalidated':
            self.add('Gilded Light','shield',self.other)
        if mode=='target_stacked':
            self.add(self.name,'second',self.owner)
        if mode=='target_suppressed':
            self.add('Stranglehold','suppressor',self.owner,'battlefield')
        if mode=='target_untap':
            for player in (0,1):
                self.add('Island',f'marker_{player}',player,'battlefield',tapped=True)
        if self.model.get('shuffle_to_owner'):
            for player, names in ((self.owner,('Plains','Mountain')),(self.other,('Forest','Island'))):
                if player==self.owner and mode=='empty_library':
                    continue
                for index,name in enumerate(names):
                    self.add(name,f'library_{player}_{index}',player,'library')

    def play_targeted(self):
        mode = self.mode
        target = self.owner if mode in ('target_self','target_stacked') else self.other
        self.cast('source',player_target=target)
        if mode=='target_countered':
            self.cast('counter',target='source',response=True)
        if mode=='target_invalidated':
            self.cast('shield',response=True)
        if mode=='target_replay':
            self.replay_from = len(self.steps)
        self.resolve()
        turns = self.model['target_turns']
        if mode=='target_stacked':
            self.play_spell('second',player_target=self.other)
            turns *= 2
        if mode=='target_untap':
            self.advance(untap=True)
            return
        self.watch_turns(turns+3)

    def play(self):
        mode = self.mode
        self.setup_cards()
        self.emit('turn',player=self.active,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')

        if mode in ('unfunded','wrong_color','cancel'):
            self.cast('source',denied=mode)
            if mode!='wrong_color':
                self.watch_turns(2)
            return
        if 'target_turns' in self.model:
            self.play_targeted()
            return
        if mode=='normal_skip':
            self.play_spell('skip')
            self.watch_turns(3)
            return
        if mode in ('older_plain','counter_extra_only'):
            self.play_spell('plain')
        if mode=='offturn_response':
            self.cast('parent')
        self.cast('source',response=mode=='offturn_response')
        if mode in ('countered','counter_extra_only'):
            self.cast('counter',target='source',response=True)
        if mode=='opposing_schedule':
            self.cast('opposition',response=True)
        if mode=='pending_replay':
            self.replay_from = len(self.steps)
        self.resolve()

        if mode=='newer_plain':
            self.play_spell('plain')
        if mode=='stacked':
            self.play_spell('second')
        if mode=='skip_meditate':
            self.play_spell('skip')
        if mode=='untap_probe':
            self.advance(untap=True)
            return
        if mode=='scheduled_replay':
            self.advance()
            self.replay_from = len(self.steps)
        if mode=='protected_end':
            assert self.advance() and self.advance() and self.active==self.other
            self.play_spell('removal',target='protection')
        self.watch_turns(4 if mode in ('stacked','older_plain','newer_plain','opposing_schedule') else 3)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    validate_witnesses(oracle,registry)
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved extra-turn source')
    cases = []
    for variant in selected:
        mode,owner = variant.rsplit('_',1)
        scene = Scene(name,model,mode,int(owner),registry,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay_from is not None:
            cut = scene.replay_from
            tail = steps[cut:]
            steps = (steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                     +tail+[step('assert_recorded_state')])
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'End-step priority before delayed loss resolution',
                            'Multiplayer and order between otherwise identical extra turns'])
        cases.append(dict(name=f'ExtraTurnSpellRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
