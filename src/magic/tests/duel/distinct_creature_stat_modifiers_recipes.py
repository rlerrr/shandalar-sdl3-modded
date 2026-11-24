"""Temporary opposite modifiers on two separately selected creatures."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'distinct_creature_stat_modifiers'
PROFILE = Path(__file__).parent / 'data/distinct_creature_stat_modifiers_wave8_profiles.json'
ORACLE = (r'Target creature gets \+([1-4])/\+([0-3]) until end of turn\. '
          r'Another target creature gets -([1-4])/-([0-3]) until end of turn\.')
COMMON = ('ordered_pair','swapped_pair','own_pair','opponent_pair','singleton','cancel_second','unfunded',
          'lost_plus','lost_minus','reentered_plus','pending_replay')
LETHAL_WITNESSES = {1:'Sakura-Tribe Elder',2:'Goblin Tinkerer',3:'Giant Scorpion'}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    effect = re.fullmatch(ORACLE,record.get('oracle_text',''))
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line') not in ('Instant','Sorcery') or record.get('keywords')!=[] or not effect):
        raise ValueError('complete normal instant or sorcery with two mandatory distinct creatures required')
    pp, pt, np, nt = map(int,effect.groups())
    return dict(schema=1,timing=record['type_line'].lower(),positive=[pp,pt],negative=[-np,-nt],distinct=True)


def variants(model):
    modes = COMMON + (('lethal_shrink',) if model['negative'][1] else ('power_only_combat',))
    if model['timing']=='instant':
        modes += ('offturn','instant_response')
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def spec(declaration, record):
    model = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(declaration.get('params'),model)):
        raise ValueError('typed whole distinct creature modifier declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('explicit distinct supported stat variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, registry):
    for group in ('sources','witnesses'):
        for name, pin in pins()[group].items():
            record, row = oracle.get(name,{}), registry.get(name,{})
            if (not exact(record,pin['oracle']) or not exact(row,pin['registry']) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal modifier {group} identity changed: {name}')


class Scene:
    def __init__(self, name, owner, mode, model, registry, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.model, self.registry, self.oracle = model, registry, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.mana = [[0]*6 for _ in (0,1)]
        self.snapshot = None

    def emit(self, op, **values):
        from recipes import step

        self.steps.append(step(op,**values))

    def add(self, ref, name, who, zone='hand'):
        from recipes import card

        self.setup.append(card(name,ref,who,zone))
        record = self.oracle[name]
        base = (int(record.get('power') or 0),int(record.get('toughness') or 0))
        self.entities[ref] = dict(name=name,player=who,zone=zone,base=base,pt=base,damage=0,tapped=False)

    def prop(self, ref, prop, value, required=False):
        self.emit('require_card' if required else 'assert_card',ref=ref,property=prop,value=int(value))

    def observe(self, ref):
        item = self.entities[ref]
        alive = item['zone']=='battlefield'
        self.prop(ref,'in_play',alive)
        if alive and 'Creature' in self.oracle[item['name']]['type_line'].split(' — ')[0]:
            for prop, value in zip(('power','toughness'),item['pt']):
                self.prop(ref,prop,value)
            for prop in ('damage','tapped'):
                self.prop(ref,prop,item[prop])

    def pair(self):
        for ref in ('plus','minus'):
            if ref in self.entities:
                self.observe(ref)

    def pools(self):
        for who, pool in enumerate(self.mana):
            for color, value in enumerate(pool):
                self.emit('assert_mana',player=who,color=color,value=value)

    def fund(self, ref):
        from recipes import cost_from_oracle

        item = self.entities[ref]
        player = item['player']
        if any(self.mana[player]):
            raise ValueError('spend the old pool before funding the next spell')
        self.mana[player] = list(cost_from_oracle(self.oracle[item['name']]))
        for color, value in enumerate(self.mana[player]):
            if value:
                self.emit('set_mana',player=player,color=color,value=value)
        self.emit('refresh')

    def cast(self, ref, targets, *, response=False, cancel_second=False):
        self.fund(ref)
        self.emit('require_can_cast',ref=ref,value=1)
        who = self.entities[ref]['player']
        if len(set(targets))!=len(targets):
            raise ValueError('target instructions must name distinct legal objects')
        for target in targets:
            self.emit('choose_target',player=who,ref=target)
        if cancel_second:
            self.emit('choose_cancel_target',player=who)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(not cancel_second))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0 if cancel_second else 2 if response else 1)
        if not cancel_second:
            self.mana[who] = [0]*6
        self.pools()

    def resolve(self, *refs, response=False):
        # resolve_response supplies the real casting context and drains the
        # complete stack, including the announced parent spell.
        self.emit('resolve_response' if response else 'resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        for ref in refs:
            self.entities[ref]['zone'] = 'graveyard'

    def main(self, player):
        self.emit('turn',player=player,value=0)

    def setup_cards(self):
        m = self.mode
        who = self.enemy if m in ('swapped_pair','opponent_pair') else self.owner
        other = self.owner if m in ('swapped_pair','own_pair') else self.enemy
        self.add('source',self.name,self.owner)
        self.add('plus','Krosan Tusker',who,'battlefield')
        if m=='singleton':
            self.add('decoy','Darksteel Relic',self.enemy,'battlefield')
            self.add('grave_creature','Ravenous Baloth',self.enemy,'graveyard')
            return
        negative = 'Ravenous Baloth'
        if m=='own_pair':
            negative = 'Krosan Tusker'
        if m=='lethal_shrink':
            negative = LETHAL_WITNESSES[-self.model['negative'][1]]
        self.add('minus',negative,other,'battlefield')
        if m in ('lost_plus','lost_minus'):
            self.add('answer','Unsummon',self.enemy)
        if m=='reentered_plus':
            self.add('answer','Cloudshift',self.owner)
        if m=='instant_response':
            self.add('precursor','Shock',self.enemy)

    def apply_modifiers(self, missing):
        for ref, sign in (('plus','positive'),('minus','negative')):
            if ref in missing:
                continue
            item = self.entities[ref]
            item['pt'] = tuple(a+b for a,b in zip(item['base'],self.model[sign]))
            if item['pt'][1]<=0:
                item['zone'] = 'graveyard'
        self.pair()

    def opponent_combat(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='minus',value=1)
        self.entities['minus']['tapped'] = True
        self.prop('minus','attacking',1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_life',player=self.owner,value=20)

    def play(self):
        m = self.mode
        self.setup_cards()
        self.main(self.enemy if m in ('offturn','instant_response','power_only_combat') else self.owner)
        self.emit('present_ai_dialogs',value=1)
        for who in (0,1):
            for color in range(6):
                self.emit('set_mana',player=who,color=color,value=0)
        self.emit('refresh')
        self.pair()
        if m=='singleton':
            self.fund('source')
            # The only in-play creature passes the single-target predicate.
            # The spell itself still requires a distinct second creature.
            self.emit('can_target_creature',ref='source',other='plus',value=1)
            self.emit('can_target_creature',ref='source',other='decoy',value=0)
            self.emit('can_cast',ref='source',value=0)
            return
        if m=='unfunded':
            self.emit('cast',ref='source',value=0)
            return
        if m=='cancel_second':
            self.cast('source',['plus'],cancel_second=True)
            return
        if m=='instant_response':
            self.cast('precursor',['plus'])
            self.emit('announce_spell',ref='precursor')
        self.cast('source',['plus','minus'],response=m=='instant_response')
        self.pair()
        if m=='pending_replay':
            self.snapshot = len(self.steps)
        missing = ()
        if m in ('lost_plus','lost_minus','reentered_plus'):
            target = 'minus' if m=='lost_minus' else 'plus'
            self.emit('announce_spell',ref='source')
            self.cast('answer',[target],response=True)
            self.resolve('answer','source',response=True)
            if m=='reentered_plus':
                csvid = self.registry[self.entities[target]['name']]['csvid']
                self.emit('bind_permanent',ref=target,player=self.owner,value=csvid)
                self.prop(target,'in_play',1,required=True)
            else:
                self.entities[target]['zone'] = 'hand'
                self.prop(target,'in_play',0)
            missing = (target,)
        elif m=='instant_response':
            self.resolve('source','precursor',response=True)
            self.entities['plus']['damage'] = 2
        else:
            self.resolve('source')
        self.apply_modifiers(missing)
        if m=='power_only_combat':
            self.opponent_combat()
        self.emit('cleanup')
        self.emit('refresh')
        for item in self.entities.values():
            item['pt'] = item['base']
            item['damage'] = 0

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        self.pair()
        for who in (0,1):
            self.emit('assert_life',player=who,value=20)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                counts = Counter(x['name'] for x in self.entities.values() if x['player']==who and x['zone']==zone)
                self.emit('assert_zone',player=who,zone=zone,value=sum(counts.values()))
                for name, amount in counts.items():
                    self.emit('assert_zone_multiplicity',player=who,zone=zone,index=self.registry[name]['csvid'],value=amount)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved distinct modifier source')
    gaps = ['Direct duplicate clicking is unavailable in the strict target input bridge. The singleton '
            'and real second-selection cancellation cells exercise the two-target requirement.']
    if model['timing']=='sorcery':
        gaps.append('Direct scripted cast does not test normal UI off-turn sorcery eligibility.')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = Scene(name,int(owner),mode,model,registry,oracle)
        scene.play()
        scene.receipts()
        steps = scene.steps
        if scene.snapshot is not None:
            cut = scene.snapshot
            tail = steps[cut:]
            steps = (steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]
                     +tail+[step('assert_recorded_state')])
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=gaps)
        cases.append(dict(name=f'DistinctCreatureStatModifiersRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
