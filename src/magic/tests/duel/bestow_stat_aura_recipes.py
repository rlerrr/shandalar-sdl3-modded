"""Printed alternate Bestow costs and persistent creature-Aura transitions."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'bestow_stat_aura'
PROFILE = Path(__file__).parent / 'data/bestow_stat_aura_wave8_profiles.json'
REMINDER = " (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)"
GREEDY_COST = r'(?:\{[1-9]\})?\{[BUGRW]\}'
MODES = ('regular_exact','regular_from_menu','no_host_regular','unfunded','bestow_own','bestow_foreign',
         'bestow_persistent','cancel_mode','cancel_target','pending_host_lost','detach_bounce','detach_death',
         'aura_destroyed','aura_bounce_regular','pending_replay','counter_bestowed','counter_regular',
         'aged_detach_attack')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text','').splitlines()
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            not record.get('type_line','').startswith('Enchantment Creature — ') or
            record.get('keywords')!=['Bestow'] or len(lines)!=2 or
            not re.fullmatch(GREEDY_COST,record.get('mana_cost','')) or not lines[0].endswith(REMINDER)):
        raise ValueError('full normal enchantment creature, printed cost and Bestow reminder required')
    price = re.fullmatch(r'Bestow ('+GREEDY_COST+r')',lines[0].removesuffix(REMINDER))
    bonus = re.fullmatch(r'Enchanted creature gets \+([1-4])/\+([1-4])\.',lines[1])
    if not price or not bonus or any(not re.fullmatch('[1-5]',record.get(p,'')) for p in ('power','toughness')):
        raise ValueError('exact Bestow cost, creature stats, and static Aura bonus required')
    return dict(schema=1,normal_cost=record['mana_cost'],bestow_cost=price[1],bonus=list(map(int,bonus.groups())),
                creature=[int(record['power']),int(record['toughness'])])


def variants(model):
    return [f'{mode}_{owner}' for owner in (0,1) for mode in MODES]


def spec(declaration, record):
    from recipes import cost_from_oracle

    model = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(declaration.get('params'),model)):
        raise ValueError('typed whole Bestow declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('explicit distinct valid Bestow variants required')
    return dict(model,family=FAMILY,normal=cost_from_oracle(record),
                bestowed=cost_from_oracle({'mana_cost':model['bestow_cost']}))


def validate_witnesses(oracle, registry):
    data = pins()
    for group in ('sources','witnesses'):
        for name, pin in data[group].items():
            record, row = oracle.get(name,{}), registry.get(name,{})
            if (not exact(record,pin['oracle']) or not exact(row,pin['registry']) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Bestow {group} identity changed: {name}')
            if group=='sources':
                aura = pin['engine_aura_face']
                if not exact(registry.get(aura['name']),aura) or aura['csvid']!=row['csvid']+1 or aura['handler']!=row['handler']:
                    raise ValueError(f'paired native Aura identity changed: {name}')


class Scene:
    def __init__(self, name, owner, mode, model, registry, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.model, self.registry, self.oracle = model, registry, oracle
        self.id = registry[name]['csvid']
        self.aura_id = pins()['sources'][name]['engine_aura_face']['csvid']
        self.setup, self.steps, self.objects = [], [], {}
        self.mana = [[0]*6 for _ in (0,1)]
        self.life = [20,20]
        self.form, self.snapshot = None, None

    def emit(self, op, **values):
        from recipes import step

        self.steps.append(step(op,**values))

    def add(self, ref, name, player, zone='hand'):
        from recipes import card

        self.setup.append(card(name,ref,player,zone))
        self.objects[ref] = dict(name=name,player=player,zone=zone,tapped=False)

    def prop(self, ref, key, value, required=False):
        self.emit('require_card' if required else 'assert_card',ref=ref,property=key,value=int(value))

    def pools(self):
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('assert_mana',player=who,color=color,value=value)

    def fund(self, who, paid):
        if any(self.mana[who]):
            raise ValueError('existing mana pool must be spent before a new fixture endowment')
        self.mana[who] = list(paid)
        for color, value in enumerate(paid):
            if value:
                self.emit('set_mana',player=who,color=color,value=value)
        self.emit('refresh')

    def spend(self, who, cost):
        self.mana[who] = [a-b for a,b in zip(self.mana[who],cost)]
        if min(self.mana[who])<0:
            raise ValueError('legal action must have its actual full price')
        self.pools()

    def cast_source(self, *, bestow=False, budget=None, choice=None, cancel_target=False, success=True):
        fee = self.model['bestowed' if bestow else 'normal']
        self.fund(self.owner,fee if budget is None else budget)
        if success:
            self.emit('require_can_cast',ref='source',value=1)
        if choice is not None:
            self.emit('choose_dialog',player=self.owner,value=choice)
        if cancel_target:
            self.emit('choose_cancel_target',player=self.owner)
        if bestow and success:
            self.emit('choose_target',player=self.owner,ref='host')
        self.emit('cast',ref='source',value=int(success))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(success))
        if success:
            self.prop('source','in_hand',0,required=True)
            self.prop('source','csvid',self.aura_id if bestow else self.id,required=True)
            self.spend(self.owner,fee)
        else:
            self.pools()
        self.host_stats()

    def cast_helper(self, ref, target=None, *, response=False):
        from recipes import cost_from_oracle

        item = self.objects[ref]
        who = item['player']
        price = cost_from_oracle(self.oracle[item['name']])
        self.fund(who,price)
        if response:
            self.emit('announce_spell',ref='source')
        else:
            self.emit('require_can_cast',ref=ref,value=1)
        if target:
            self.emit('choose_target',player=who,ref=target)
        self.emit('cast_response' if response else 'cast',ref=ref,value=1)
        self.emit('assert_stack',value=2 if response else 1)
        self.emit('assert_choices_consumed')
        self.spend(who,price)
        self.resolve(response=response)
        item['zone'] = 'graveyard'

    def resolve(self, response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)

    def host_stats(self):
        for ref in ('host','other'):
            if ref not in self.objects or self.objects[ref]['zone']!='battlefield':
                continue
            card = self.oracle[self.objects[ref]['name']]
            buffs = self.model['bonus'] if self.form=='aura' and ref=='host' else (0,0)
            for prop, bonus in zip(('power','toughness'),buffs):
                self.prop(ref,prop,int(card[prop])+bonus)

    def source_state(self):
        self.prop('source','in_play',1,required=True)
        aura = self.form=='aura'
        self.prop('source','csvid',self.aura_id if aura else self.id)
        self.emit('can_target_creature',ref='probe',other='source',value=int(not aura))
        if not aura:
            for key, value in zip(('power','toughness'),self.model['creature']):
                self.prop('source',key,value)
        self.host_stats()

    def entered(self, form):
        self.form = form
        self.objects['source']['zone'] = 'battlefield'
        self.source_state()

    def age_turns(self):
        if any(map(any,self.mana)):
            raise ValueError('actual untap and upkeep need empty mana pools')
        for player in (self.enemy,self.owner):
            self.emit('advance_turn')
            self.emit('untap_step',player=player)
            self.emit('upkeep_step',player=player)
            self.emit('turn',player=player,value=0)
            self.source_state()

    def attack(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.prop('source','attacking',1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        self.objects['source']['tapped'] = True
        self.life[self.enemy] -= self.model['creature'][0]

    def setup_cards(self):
        m = self.mode
        self.add('source',self.name,self.owner)
        self.add('probe','Murder',self.enemy)
        if m=='no_host_regular':
            self.add('relic','Darksteel Relic',self.owner,'battlefield')
        else:
            host = self.enemy if m=='bestow_foreign' else self.owner
            self.add('host','Krosan Tusker',host,'battlefield')
            self.add('other','Ravenous Baloth',1-host,'battlefield')
        helpers = {'pending_host_lost':'Unsummon','detach_bounce':'Unsummon','detach_death':'Murder',
                   'aged_detach_attack':'Unsummon','aura_destroyed':'Naturalize','aura_bounce_regular':'Boomerang',
                   'counter_bestowed':'Negate','counter_regular':'Essence Scatter'}
        if m in helpers:
            self.add('helper',helpers[m],self.enemy)

    def play(self):
        m = self.mode
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            for color in range(6):
                self.emit('set_mana',player=who,color=color,value=0)
        self.emit('refresh')
        if m=='unfunded':
            short = self.model['normal'][:]
            color = next(n for n,v in enumerate(short) if n and v)
            short[color] -= 1
            self.cast_source(budget=short,success=False)
            return
        if m in ('cancel_mode','cancel_target'):
            self.cast_source(budget=self.model['bestowed'],choice=2 if m=='cancel_mode' else 0,
                             cancel_target=m=='cancel_target',success=False)
            return
        base = m in ('regular_exact','regular_from_menu','no_host_regular','counter_regular')
        if base:
            overfunded = m in ('regular_from_menu','no_host_regular')
            self.cast_source(budget=self.model['bestowed'] if overfunded else None,
                             choice=1 if m=='regular_from_menu' else None)
        else:
            self.cast_source(bestow=True,choice=0)
        if m in ('counter_bestowed','counter_regular'):
            self.cast_helper('helper',response=True)
            self.objects['source']['zone'] = 'graveyard'
            return
        if m=='pending_host_lost':
            self.cast_helper('helper','host',response=True)
            self.objects['host']['zone'] = 'hand'
            self.entered('creature')
            return
        if m=='pending_replay':
            self.snapshot = len(self.steps)
        self.resolve()
        self.entered('creature' if base else 'aura')
        if m in ('bestow_persistent','aged_detach_attack','pending_replay'):
            self.age_turns()
        if m in ('detach_bounce','detach_death','aged_detach_attack'):
            self.cast_helper('helper','host')
            self.objects['host']['zone'] = 'graveyard' if m=='detach_death' else 'hand'
            self.entered('creature')
            if m=='aged_detach_attack':
                self.attack()
        if m=='aura_destroyed' or m=='aura_bounce_regular':
            self.cast_helper('helper','source')
            self.form = None
            self.objects['source']['zone'] = 'graveyard' if m=='aura_destroyed' else 'hand'
            self.host_stats()
            if m=='aura_bounce_regular':
                self.emit('bind_hand',ref='source',player=self.owner,value=self.id)
                self.prop('source','in_hand',1,required=True)
                self.cast_source()
                self.resolve()
                self.entered('creature')

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        self.host_stats()
        for ref, item in self.objects.items():
            self.prop(ref,'in_play',item['zone']=='battlefield')
        if self.form=='creature':
            self.prop('source','tapped',self.objects['source']['tapped'])
        for who, life in enumerate(self.life):
            self.emit('assert_life',player=who,value=life)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                counts = Counter(self.aura_id if ref=='source' and self.form=='aura' else self.registry[x['name']]['csvid']
                                 for ref,x in self.objects.items() if x['player']==who and x['zone']==zone)
                self.emit('assert_zone',player=who,zone=zone,value=sum(counts.values()))
                for csvid, value in counts.items():
                    self.emit('assert_zone_multiplicity',player=who,zone=zone,index=csvid,value=value)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved Bestow source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = Scene(name,int(owner),mode,model,registry,oracle)
        scene.play()
        scene.receipts()
        actions = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = actions[cut:]
            actions = (actions[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                       +tail+[step('assert_recorded_state')])
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'External cost modifiers and casting without paying the mana cost',
                            'Aura exchanges and multi-controller changes while attached'])
        cases.append(dict(name=f'BestowStatAuraRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=actions,_recipe=metadata))
    return cases
