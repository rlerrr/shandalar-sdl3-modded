"""Paid Heroic target choices and their control, combat and movement effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counted_battlefield_damage_recipes import types
from death_counter_trigger_recipes import exact
from heroic_case_support import HeroicBoard, heroic_rules


FAMILY = 'heroic_target_control_riders'
PROFILE = Path(__file__).parent/'data/heroic_target_control_riders_wave8_profiles.json'
PREFIX = 'Heroic — Whenever you cast a spell that targets this creature, '
RULES = {
    'gain control of another target creature until end of turn. Untap that creature. It gains haste until end of turn.':'steal',
    "target creature an opponent controls can't block this turn.":'deny_block',
    'you may tap target creature.':'tap',
    'destroy target enchantment creature an opponent controls.':'destroy',
    "you may return target enchantment to its owner's hand.":'bounce',
    "tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step.":'freeze',
    'return target creature card with mana value 2 or less from your graveyard to the battlefield.':'return',
}
COMMON = ('own','opponent','other_target','countered','replay')
EXTRAS = {
    'steal': ('haste_return','own_victim','no_target'),
    'deny_block': ('combat','combat_negative','expires','no_target'),
    'tap': ('own_victim','already_tapped'),
    'destroy': ('typed','no_target'),
    'bounce': ('foreign_owner','own_victim','no_target'),
    'freeze': ('untap_window','already_tapped','no_target'),
    'return': ('mv_two','mv_zero','no_target'),
}
VANILLA = {'Grizzly Bears','Hill Giant','Centaur Courser','Ornithopter'}
WITNESSES = VANILLA | {
    'Defiant Strike','Counterspell','Forest','Island','Plains','Mountain','Fervor','Jalum Tome','Nyxborn Shieldmate',
}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines = heroic_rules(record)
    effect = RULES.get(lines[0].removeprefix(PREFIX)) if len(lines)==1 and lines[0].startswith(PREFIX) else None
    if not effect or record['keywords']!=['Heroic']:
        raise ValueError('whole exact Heroic target-control rider required')
    return dict(effect=effect,price=cost_from_oracle(record),power=int(record['power']),toughness=int(record['toughness']))


def variants(shape):
    effect = shape['effect']
    choices = [f'{mode}_{seat}' for seat in (0,1) for mode in COMMON+EXTRAS[effect]]
    return choices + (['decline_0'] if effect in ('tap','bounce') else [])


def spec(declaration,record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profiles()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),shape)):
        raise ValueError('exact pinned whole Heroic source, cost and rider required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct reviewed Heroic variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins = profiles()
    if set(pins['witnesses'])!=WITNESSES or len(pins['sources'])!=len(RULES):
        raise ValueError('complete pinned source and witness scope required')
    for name,pin in (pins['sources']|pins['witnesses']).items():
        row = rows[name]
        if (not exact(pin['oracle'],oracle[name]) or not exact(pin['registry'],row) or row['kind']!='card'
                or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in VANILLA):
            raise ValueError(f'independent Heroic identity changed: {name}')


class TargetControlBoard(HeroicBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.effect = shape['effect']
        self.life = [20,20]
        self.active = owner
        self.target = None
        self.rebound_ref = None
        self.untapped_through = False
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode,effect = self.mode,self.effect
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')
        if effect=='return':
            self.build_graveyards()
        elif effect=='destroy':
            self.add('Nyxborn Shieldmate','own_aura_creature',self.owner)
            self.add('Fervor','enchantment_only',self.foe)
            self.add('Grizzly Bears','creature_only',self.foe)
            if mode!='no_target':
                self.add('Nyxborn Shieldmate','victim',self.foe)
        elif effect=='bounce':
            if mode!='no_target':
                target_player = self.owner if mode=='own_victim' else self.foe
                self.add('Fervor','victim',target_player)
                if mode=='foreign_owner':
                    self.setup[-1].setdefault('state',[]).append('opponent_owned')
            self.add('Jalum Tome','wrong_type',self.foe)
        elif mode!='no_target':
            target_player = self.owner if mode=='own_victim' else self.foe
            self.add('Grizzly Bears','victim',target_player,
                     tapped=effect=='steal' or mode=='already_tapped')
        if effect=='deny_block' and mode in ('combat','combat_negative','expires') or mode=='other_target':
            self.add('Hill Giant','other',self.owner)
        if effect=='deny_block' and mode=='no_target':
            self.add('Grizzly Bears','ally',self.owner)
        if effect=='freeze' and mode=='no_target':
            self.add('Grizzly Bears','ally',self.owner)
        for who in (0,1):
            for index,name in enumerate(('Forest','Island','Plains','Mountain')):
                self.add(name,f'library_{who}_{index}',who,'library')

    def build_graveyards(self):
        if self.mode!='no_target':
            self.add('Grizzly Bears','grave_two',self.owner,'graveyard')
            self.add('Ornithopter','grave_zero',self.owner,'graveyard')
        self.add('Centaur Courser','grave_three',self.owner,'graveyard')
        self.add('Forest','grave_land',self.owner,'graveyard')
        self.add('Grizzly Bears','opposing_grave',self.foe,'graveyard')

    def stat(self,ref,property,value):
        self.emit('assert_card',ref=ref,property=property,value=int(value))

    def bind(self,ref):
        card = self.cards[ref]
        self.emit('bind_permanent_copy',ref=ref,player=card['player'],index=0,value=self.rows[card['name']]['csvid'])
        self.rebound_ref = ref

    def chooser(self,board):
        assert board is self
        ref = self.target
        if not ref:
            return
        if self.effect=='return':
            self.emit('choose_zone_card',player=self.owner,value=self.rows[self.cards[ref]['name']]['csvid'])
        else:
            if self.effect=='tap' and self.owner==0:
                self.emit('choose_trigger_source',player=0,ref='source')
            self.emit('choose_optional_target' if self.mode=='decline' else 'choose_target',
                      player=self.owner,ref=ref)

    def before_counter(self,board):
        assert board is self
        self.cast('counter',response=True)
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.cards['counter']['zone'] = self.cards['trigger']['zone'] = 'graveyard'

    def expected_effect(self):
        if not self.target or self.mode=='decline':
            if self.mode=='decline':
                self.emit('assert_forced_target_choices',value=0)
            return
        victim = self.cards[self.target]
        effect = self.effect
        if effect=='steal':
            victim['player'] = self.owner
            victim['tapped'] = False
        elif effect in ('freeze','tap'):
            victim['tapped'] = True
        elif effect=='destroy':
            victim['zone'] = 'graveyard'
        elif effect=='bounce':
            if self.mode=='foreign_owner':
                victim['player'] = self.owner
            victim['zone'] = 'hand'
        elif effect=='return':
            self.offer_receipts()
            victim['zone'] = 'battlefield'

    def offer_receipts(self):
        grave = [card['name'] for ref,card in self.cards.items() if ref not in ('trigger','counter')
                 and card['player']==self.owner and card['zone']=='graveyard']
        self.emit('assert_offered_count',value=len(grave))
        self.emit('assert_offered_selection_required',value=1)
        for name in sorted(set(grave)):
            eligible = 'Creature' in types(self.oracle[name]) and self.oracle[name]['cmc']<=2
            self.emit('assert_offered_legal_card',index=self.rows[name]['csvid'],value=int(eligible))

    def trigger(self):
        own = self.mode not in ('opponent','combat_negative')
        desired_source = 'other' if self.mode=='other_target' else 'source'
        actor = self.owner if own else self.foe
        if own and desired_source=='source' and self.mode!='no_target':
            if self.effect=='return':
                self.target = 'grave_zero' if self.mode=='mv_zero' else 'grave_two'
                self.emit('observe_search_filters',value=1)
            else:
                self.target = 'victim'
        countered = self.mode=='countered'
        self.heroic_spell(own,target=desired_source,choices=self.chooser,resolve=not countered,
                          before_resolve=self.before_counter if countered else None)
        if not countered:
            self.draw(actor)
            self.cards[desired_source]['boost'] = 1
        self.expected_effect()

    def receipts(self):
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)
        self.observe_mana()
        quantities = Counter((card['player'],card['zone'],self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone in ('battlefield','hand','graveyard','exile','library'):
                self.emit('assert_zone',player=who,zone=zone,
                          value=sum(count for (p,z,_),count in quantities.items() if p==who and z==zone))
            names = [card['name'] for card in self.cards.values() if card['player']==who and card['zone']=='library']
            for index,name in enumerate(names):
                self.emit('assert_zone_card',player=who,zone='library',index=index,value=self.rows[name]['csvid'])
        for (who,zone,csvid),amount in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity',player=who,zone=zone,index=csvid,value=amount)
        for ref,card in self.cards.items():
            if card['zone']!='battlefield':
                continue
            changed_ref = self.effect=='steal' and ref=='victim' and self.target or ref.startswith('grave_')
            if changed_ref and ref!=self.rebound_ref:
                self.emit('assert_matching_permanents',player=card['player'],index=self.rows[card['name']]['csvid'],
                          property='tapped',value=int(card['tapped']))
                if ref.startswith('grave_'):
                    self.emit('assert_matching_permanents',player=card['player'],index=self.rows[card['name']]['csvid'],
                              property='sick',value=1)
                continue
            self.stat(ref,'in_play',1)
            self.stat(ref,'tapped',card['tapped'])
            data = self.oracle[card['name']]
            if 'Creature' in types(data):
                self.stat(ref,'power',int(data['power'])+card['boost'])
                self.stat(ref,'toughness',int(data['toughness']))
                self.stat(ref,'damage',card['damage'])

    def clear_end_turn(self):
        for card in self.cards.values():
            card['damage'] = card['boost'] = 0

    def advance(self,*,keep_tapped=None):
        if any(map(any,self.pools)):
            raise ValueError('untap contrast requires genuinely empty mana pools')
        self.emit('advance_turn')
        self.active = 1-self.active
        self.clear_end_turn()
        self.emit('untap_step',player=self.active)
        for ref,card in self.cards.items():
            if card['zone']=='battlefield' and card['player']==self.active and ref!=keep_tapped:
                card['tapped'] = False
        self.emit('turn',player=self.active,value=0)

    def combat(self,attacker,blocker=None,blocked=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.cards[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        power = int(self.oracle[self.cards[attacker]['name']]['power'])+self.cards[attacker]['boost']
        if blocked:
            self.cards[blocker]['zone'] = 'graveyard'
            self.cards[attacker]['damage'] = int(self.oracle[self.cards[blocker]['name']]['power'])
        else:
            self.life[self.foe] -= power
        self.receipts()

    def play(self):
        self.play_source()
        self.trigger()
        self.receipts()
        if self.mode=='haste_return':
            self.bind('victim')
            self.combat('victim')
            self.emit('cleanup')
            self.emit('refresh')
            self.cards['victim']['player'] = self.foe
            self.rebound_ref = None
            self.clear_end_turn()
            self.receipts()
        if self.mode in ('combat','combat_negative','expires'):
            if self.mode=='expires':
                self.advance()
                self.advance()
            self.combat('other','victim',blocked=self.mode!='combat')
        if self.mode=='untap_window':
            self.advance(keep_tapped='victim')
            self.receipts()
            self.advance()
            self.advance()
            self.receipts()


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('unbound Heroic source or variant')
    gaps = ['Duel-only player scope and control changes before the actual triggering cast.']
    if shape['effect'] in ('tap','bounce'):
        gaps.append('Declining an optional effect is scripted only for the human seat.')
    meta = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
    cases = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board = TargetControlBoard(name,shape,mode,int(seat),rows,oracle)
        board.play()
        steps = board.steps
        if mode=='replay':
            start = next(i for i,item in enumerate(steps) if item==step('assert_card',ref='source',property='in_play',value=1))
            start += 2
            tail = steps[start:]
            steps = [*steps[:start],step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                     step('restore'),*tail,step('assert_recorded_state')]
        cases.append(dict(name=f'HeroicTargetControlRiderRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=2119,
                          setup=board.setup,steps=steps,_recipe=dict(meta,variant=variant)))
    return cases
