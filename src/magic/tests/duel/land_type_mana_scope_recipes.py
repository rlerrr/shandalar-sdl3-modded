"""Whole Oracle static land changes observed through domain and real mana."""
from collections import Counter
import copy
import json
from pathlib import Path
import re

from counter_condition_recurrence_recipes import exact


FAMILY = 'land_type_mana_scope'
BASIC_TYPES = dict(Swamp='B', Island='U', Forest='G', Mountain='R', Plains='W')
SWAMP_TEXT = 'Each land is a Swamp in addition to its other land types.'
PRESENCE_TEXT = ('Enchant land\nWhen this Aura enters, draw a card.\n'
                 'Enchanted land is every basic land type in addition to its other types.')
COMMON = ('domain', 'owner_basic', 'owner_nonbasic', 'enemy_basic', 'enemy_nonbasic',
          'later_own_entry', 'later_enemy_entry', 'restore_after_removal', 'pending_replay',
          'nonland_unchanged', 'repeat_denied')


def pins():
    data = Path(__file__).parent/'data/land_type_mana_scope_wave8_profiles.json'
    return json.loads(data.read_text(encoding='utf8'))


def oracle_model(record):
    if record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('single normal land-type source required')
    text, type_line = record['oracle_text'], record['type_line']
    if text==SWAMP_TEXT and type_line in ('Enchantment','Legendary Land'):
        modifier = dict(scope='all_lands', update='add', types=['Swamp'], draws=0)
    elif text=='Nonbasic lands are Mountains.' and (type_line=='Enchantment' or type_line.startswith('Creature — ')):
        modifier = dict(scope='nonbasic_lands', update='replace', types=['Mountain'], draws=0)
    elif text=='Lands you control are every basic land type in addition to their other types.' and type_line=='Enchantment':
        modifier = dict(scope='own_lands', update='add', types=list(BASIC_TYPES), draws=0)
    elif text==PRESENCE_TEXT and type_line=='Enchantment — Aura':
        modifier = dict(scope='enchanted_land', update='add', types=list(BASIC_TYPES), draws=1)
    else:
        raise ValueError('unreviewed complete static land-type clauses')
    form = {'Legendary Land':'land', 'Enchantment':'enchantment', 'Enchantment — Aura':'aura'}.get(type_line, 'creature')
    keywords = ['Enchant'] if form=='aura' else []
    if record.get('keywords')!=keywords or (form=='land')!=(record.get('mana_cost')==''):
        raise ValueError('land-type card form and metadata disagree')
    if form=='creature' and any(not re.fullmatch(r'[1-9][0-9]*', record.get(prop, '')) for prop in ('power','toughness')):
        raise ValueError('real printed creature characteristics required')
    return dict(form=form, **modifier)

def variants(model):
    extra = []
    if model['form']=='land':
        extra += ['source_self_mana']
    else:
        extra += ['unfunded']
    if model['update']=='add':
        extra += ['original_mana']
    if len(model['types'])==5:
        extra += ['color_'+color.lower() for color in BASIC_TYPES.values()]
    if model['update']=='replace':
        extra += ['preserve_artifact_type']
    if model['form']=='aura':
        extra += ['foreign_target', 'no_land_target']
    return [f'{mode}_{actor}' for actor in (0,1) for mode in (*COMMON,*extra)]


def spec(declaration, record):
    from recipes import cost_from_oracle
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int
            or declaration['version']!=2 or declaration.get('oracle_text')!=record.get('oracle_text')):
        raise ValueError('whole Oracle version-two land-type recipe required')
    model = oracle_model(record)
    if not exact(declaration.get('params'),dict(schema=8,**model)):
        raise ValueError('explicit land-type profile must match full Oracle')
    requested = declaration.get('variants')
    if (not isinstance(requested,list) or not requested or any(not isinstance(v,str) for v in requested)
            or len(set(requested))!=len(requested) or set(requested)-set(variants(model))):
        raise ValueError('distinct supported land-type variants required')
    return dict(model, family=FAMILY, cast_fee=None if model['form']=='land' else cost_from_oracle(record))


def validate_witnesses(oracle, rows):
    data = pins()
    for name,pin in (data['sources'] | data['witnesses']).items():
        row = rows.get(name,{})
        if (not exact(pin['oracle'],oracle.get(name)) or not exact(pin['registry'],row) or row.get('kind')!='card'
                or row.get('handler') in ('card_dummy','0','NULL',None)):
            raise ValueError('complete implemented card pin changed: '+name)
    return data


class LandTypeCase:
    def __init__(self, name, declaration, model, variant, rows, oracle):
        from recipes import card, step, cost_from_oracle, COLORS
        self.card, self.step, self.cost = card, step, cost_from_oracle
        self.colors = {'C':0, **COLORS}
        self.name, self.model, self.declaration = name, model, declaration
        self.variant, self.rows, self.oracle = variant, rows, oracle
        self.mode, owner = variant.rsplit('_',1)
        self.owner = int(owner)
        self.enemy = 1-self.owner
        self.focus = 'enemy_nonbasic' if self.mode=='foreign_target' else 'owner_nonbasic'
        self.zones = [{zone:[] for zone in ('hand','library','battlefield','graveyard','exile')} for _ in (0,1)]
        self.setup, self.entities, self.steps = [], {}, []
        self.pools = [[0]*6, [0]*6]
        self.active = False
        self.replay_cut = None
        self.prepare_setup()

    def emit(self, op, **kw):
        self.steps.append(self.step(op, **kw))

    def add(self, name, ref, actor, zone='hand'):
        item = self.card(name, ref, actor, zone)
        if zone=='battlefield':
            item['state'] = ['untapped']
        self.setup.append(item)
        self.entities[ref] = dict(name=name, player=actor, zone=zone)
        self.zones[actor][zone].append(name)

    def prepare_setup(self):
        self.add(self.name, 'source', self.owner)
        if self.mode not in ('foreign_target','no_land_target'):
            self.add('Forest','owner_basic',self.owner,'battlefield')
            self.add('Seat of the Synod','owner_nonbasic',self.owner,'battlefield')
            if self.mode=='unfunded':
                for entry in self.setup[-2:]:
                    entry['state'] = ['tapped']
            self.add('Matca Rioters','owner_domain',self.owner,'battlefield')
        if self.mode!='no_land_target':
            self.add('Plains','enemy_basic',self.enemy,'battlefield')
            if self.mode!='preserve_artifact_type':
                self.add('Tree of Tales','enemy_nonbasic',self.enemy,'battlefield')
            self.add('Matca Rioters','enemy_domain',self.enemy,'battlefield')
        for actor in (0,1):
            self.add('Swamp',f'grave_{actor}',actor,'graveyard')
            self.add('Island',f'exile_{actor}',actor,'exile')
        self.add('Darksteel Relic','draw_0',self.owner,'library')
        self.add('Elvish Mystic','draw_1',self.owner,'library')
        if self.model['form']=='land':
            self.add('Walking Atlas','source_atlas',self.owner,'battlefield')
        if self.mode in ('later_own_entry','later_enemy_entry'):
            actor = self.owner if self.mode=='later_own_entry' else self.enemy
            self.add('Walking Atlas','entrant_atlas',actor,'battlefield')
            self.add('Tree of Tales','entrant',actor)
        if self.mode=='nonland_unchanged' or self.mode=='no_land_target':
            self.add('Sol Ring','nonland',self.owner,'battlefield')
        if self.mode=='restore_after_removal':
            removal, actor = {
                'land':('Sinkhole',self.owner), 'creature':('Murder',self.enemy),
                'aura':('Naturalize',self.enemy), 'enchantment':('Naturalize',self.enemy),
            }[self.model['form']]
            self.add(removal,'remove_source',actor)
        if self.mode=='preserve_artifact_type':
            self.add('Shatter','shatter',self.enemy)

    def move(self, ref, zone):
        entity = self.entities[ref]
        zones, name = self.zones[entity['player']], entity['name']
        if entity['zone']!='stack':
            zones[entity['zone']].remove(name)
        if zone!='stack':
            zones[zone].append(name)
        entity['zone'] = zone

    def fund(self, player, amounts):
        self.pools[player] = list(amounts)
        for color, amount in enumerate(amounts):
            self.emit('set_mana',player=player,color=color,value=amount)
        self.emit('refresh')

    def check_pools(self):
        for player, amounts in enumerate(self.pools):
            for color, amount in enumerate(amounts):
                self.emit('assert_mana',player=player,color=color,value=amount)

    def cast_pending(self, ref, fee, target=None, *, allowed=True, guard=None):
        actor = self.entities[ref]['player']
        self.fund(actor,fee)
        if guard is not None:
            self.emit('require_can_cast',ref=ref,value=int(guard))
        if target is not None:
            self.emit('choose_target',player=actor,ref=target)
        self.emit('cast',ref=ref,value=int(allowed))
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.move(ref,'stack')
            self.pools[actor] = [0]*6
        self.check_pools()

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')

    def atlas(self, atlas, land):
        actor = self.entities[land]['player']
        self.emit('require_can_activate',ref=atlas,value=1)
        self.emit('activate',ref=atlas,value=1)
        self.emit('assert_stack',value=1)
        self.emit('assert_card',ref=atlas,property='tapped',value=1)
        self.emit('assert_card',ref=land,property='in_hand',value=1)
        if land=='source' and self.mode=='pending_replay':
            self.replay_cut = len(self.steps)
        self.emit('choose_zone_card',player=actor,value=self.rows[self.entities[land]['name']]['csvid'])
        self.resolve()
        self.move(land,'battlefield')
        self.emit('assert_card',ref=land,property='in_play',value=1)
        self.emit('assert_card',ref=land,property='tapped',value=0)

    def modifier_applies(self, ref):
        if not self.active:
            return False
        entity = self.entities[ref]
        record = self.oracle[entity['name']]
        type_head = record['type_line'].partition(' — ')[0].split()
        if 'Land' not in type_head:
            return False
        scope = self.model['scope']
        return {'all_lands':True, 'nonbasic_lands':'Basic' not in type_head,
                'own_lands':entity['player']==self.owner, 'enchanted_land':ref==self.focus}[scope]

    def domain_types(self, ref):
        record = self.oracle[self.entities[ref]['name']]
        types = set(record['type_line'].partition(' — ')[2].split()) & set(BASIC_TYPES)
        if not self.modifier_applies(ref):
            return types
        additions = set(self.model['types'])
        return additions if self.model['update']=='replace' else types | additions

    def check_domain(self):
        for player, receiver in ((self.owner,'owner_domain'),(self.enemy,'enemy_domain')):
            if receiver not in self.entities:
                continue
            types = set()
            for ref, entity in self.entities.items():
                if entity['player']==player and entity['zone']=='battlefield':
                    types.update(self.domain_types(ref))
            for prop in ('power','toughness'):
                self.emit('assert_card',ref=receiver,property=prop,value=len(types))

    def receivers(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.check_pools()
        self.check_domain()
        for player, zones in enumerate(self.zones):
            self.emit('assert_life',player=player,value=20)
            for zone, names in zones.items():
                self.emit('assert_zone',player=player,zone=zone,value=len(names))
                counts = Counter(names)
                for name in sorted({item['name'] for item in self.setup}):
                    # Type-changing callbacks may expose a replacement IID for
                    # a live land. Its object is tracked by ref, not that IID.
                    if zone=='battlefield' and 'Land' in self.oracle[name]['type_line'].partition(' — ')[0].split():
                        continue
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,
                              index=self.rows[name]['csvid'],value=counts[name])
                if zone=='library':
                    for index,name in enumerate(names):
                        self.emit('assert_zone_card',player=player,zone='library',index=index,value=self.rows[name]['csvid'])
        for ref, entity in self.entities.items():
            if entity['zone']=='battlefield':
                self.emit('assert_card',ref=ref,property='in_play',value=1)
        if self.active and self.model['form']=='creature':
            for prop in ('power','toughness'):
                self.emit('assert_card',ref='source',property=prop,value=int(self.oracle[self.name][prop]))

    def cast_source(self):
        if self.model['form']=='land':
            self.atlas('source_atlas','source')
        else:
            fee = self.model['cast_fee']
            allowed = self.mode not in ('unfunded','no_land_target')
            if self.mode=='unfunded':
                fee = [0]*6
            target = self.focus if self.model['form']=='aura' and allowed else None
            guard = True if self.mode=='foreign_target' else False if self.mode=='no_land_target' else None
            self.cast_pending('source',fee,target,allowed=allowed,guard=guard)
            if not allowed:
                return
            self.check_domain()
            if self.mode=='pending_replay':
                self.replay_cut = len(self.steps)
            self.resolve()
            self.move('source','battlefield')
            if self.model['draws']:
                self.move('draw_0','hand')
        self.active = True

    def palette(self, ref):
        record = self.oracle[self.entities[ref]['name']]
        original = set(re.findall(r'Add \{([CBUGRW])\}',record['oracle_text']))
        if self.modifier_applies(ref) and self.model['update']=='replace':
            original.clear()
        return original | {BASIC_TYPES[t] for t in self.domain_types(ref)}

    def mana(self, ref, desired=None):
        entity = self.entities[ref]
        player = entity['player']
        palette = self.palette(ref)
        order = ('B','R','G','U','W','C')
        color = desired or next(c for c in order if c in palette)
        if color not in palette:
            raise ValueError('chosen mana outside the Oracle palette')
        provider = self.model['form']=='aura' and self.modifier_applies(ref)
        if provider:
            self.emit('can_activate_granted_mana',ref=ref,other='source',value=1)
        else:
            self.emit('require_can_activate',ref=ref,value=1)
        if len(palette)>1:
            self.emit('choose_color',player=player,value=self.colors[color])
        if provider:
            self.emit('activate_granted_mana',ref=ref,other='source',value=1)
        else:
            self.emit('activate',ref=ref,value=1)
        self.pools[player][self.colors[color]] += 2 if entity['name']=='Sol Ring' else 1
        self.emit('assert_stack',value=0)
        self.emit('assert_card',ref=ref,property='tapped',value=1)
        self.check_pools()
        self.emit('assert_choices_consumed')
        if self.mode=='repeat_denied':
            if provider:
                self.emit('can_activate_granted_mana',ref=ref,other='source',value=0)
            else:
                self.emit('require_can_activate',ref=ref,value=0)
                self.emit('activate',ref=ref,value=0)

    def remove(self, ref, victim, guard=None):
        name = self.entities[ref]['name']
        self.cast_pending(ref,self.cost(self.oracle[name]),target=victim,guard=guard)
        self.resolve()
        self.move(ref,'graveyard')
        self.move(victim,'graveyard')
        self.emit('assert_card',ref=victim,property='in_play',value=0)

    def build(self):
        self.emit('turn',player=self.owner,value=0)
        for player in (0,1):
            self.fund(player,[0]*6)
        self.check_domain()
        self.cast_source()
        if self.active:
            self.check_domain()
            self.exercise()
        self.receivers()
        if self.replay_cut is not None:
            cut = self.replay_cut
            tail = self.steps[cut:]
            self.steps = (self.steps[:cut]+[self.step('capture')]+tail+
                          [self.step('record_state'),self.step('ai_roundtrip'),self.step('restore')]+
                          copy.deepcopy(tail)+[self.step('assert_recorded_state')])
        csvid = self.rows[self.name]['csvid']
        return dict(name=f'LandTypeManaScopeRecipe_{csvid}_{self.variant}',card=self.name,
                    fixture='empty_main',seed=123,setup=self.setup,steps=self.steps,
                    _recipe=dict(family=FAMILY,version=2,variant=self.variant,params=self.declaration['params'],
                                 oracle_text=self.declaration['oracle_text'],
                                 untested_components=['Interactions among concurrent type-changing continuous effects',
                                                      'Manual land play limits and multiplayer']))

    def exercise(self):
        if self.mode in ('owner_basic','owner_nonbasic','enemy_basic','enemy_nonbasic'):
            self.mana(self.mode)
        elif self.mode in ('later_own_entry','later_enemy_entry'):
            self.atlas('entrant_atlas','entrant')
            self.mana('entrant')
        elif self.mode=='restore_after_removal':
            self.remove('remove_source','source')
            self.active = False
            self.check_domain()
            self.mana('owner_nonbasic','U')
        elif self.mode=='nonland_unchanged':
            self.mana('nonland','C')
        elif self.mode=='repeat_denied':
            self.mana('owner_nonbasic')
        elif self.mode=='original_mana':
            self.mana('owner_nonbasic','U')
        elif self.mode.startswith('color_'):
            self.mana('owner_nonbasic',self.mode[-1].upper())
        elif self.mode=='source_self_mana':
            self.mana('source','B')
        elif self.mode=='preserve_artifact_type':
            self.remove('shatter','owner_nonbasic',guard=True)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    data = validate_witnesses(oracle,registry)
    if data['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved land-type source')
    return [LandTypeCase(name,declaration,model,v,registry,oracle).build() for v in selected]
