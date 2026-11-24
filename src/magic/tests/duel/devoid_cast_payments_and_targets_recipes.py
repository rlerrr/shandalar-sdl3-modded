"""Paid Devoid spells and their real target, payment and control effects."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path

from counted_battlefield_damage_recipes import types
from death_counter_trigger_recipes import exact
from devoid_case_support import DevoidBoard, devoid_rules


FAMILY = 'devoid_cast_payments_and_targets'
PROFILE = Path(__file__).parent/'data/devoid_cast_payments_and_targets_wave8_profiles.json'
COLORLESS = ' ({C} represents colorless mana.)'
RULES = {
    ('When you cast this spell, you may pay {1}{C}. If you do, target opponent sacrifices a creature.'+COLORLESS,
     'Flying',"This creature can't block."): ('sacrifice', ('Flying',)),
    ("{2}{C}: Exile another target creature, then return it to the battlefield tapped under its owner's control."+COLORLESS,):
        ('blink', ()),
    ('When you cast this spell, you may pay {1}{C}. If you do, gain control of target creature until end of turn, '
     'untap that creature, and it gains haste until end of turn.'+COLORLESS,'Haste'): ('steal', ('Haste',)),
    ("{2}{C}: Each creature with toughness greater than its power can't block this turn."+COLORLESS,):
        ('block', ()),
    ('Flash (You may cast this spell any time you could cast an instant.)',
     'When this creature enters, another target creature you control gains hexproof until end of turn.'):
        ('ward', ('Flash',)),
    ('When you cast this spell, exile target artifact, enchantment, or land.','Reach',
     '{2}{C}, Sacrifice a land: Return this card from your graveyard to your hand.'+COLORLESS):
        ('exile', ('Reach',)),
    ('Enchant creature',"Enchanted creature can't block.",
     'Whenever enchanted creature deals damage, its controller loses that much life.'):
        ('aura', ('Enchant',)),
}
MODES = {
    'sacrifice': ('paid','decline','colored_only','empty_foe','countered','flight','cant_block','replay'),
    'blink': ('ordinary','foreign_owner','own_target','only_self','colored_only','short','offturn','replay'),
    'steal': ('paid','decline','colored_only','haste','paid_attack','countered','own_target','replay'),
    'block': ('tall','equal','other_attacker','own_defender','colored_only','short','expiry','replay'),
    'ward': ('own_target','offturn','foe_only','response','expiry','self_only','replay'),
    'exile': ('exile_artifact','exile_enchantment','exile_land','creature_only','countered','reach','replay',
              'return_paid','return_wrong_color','return_short','return_no_land','return_offturn'),
    'aura': ('noncombat','combat','cant_block','own_host','other_source','no_creature','replay'),
}
VANILLA = {'Grizzly Bears','Hill Giant','Giant Spider','Wind Drake'}
WITNESSES = VANILLA | {'Counterspell','Unsummon','Jalum Tome','Fervor','Forest','Prodigal Sorcerer','Wastes'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    role = RULES.get(tuple(devoid_rules(record)))
    if not role:
        raise ValueError('complete reviewed Devoid effect required')
    effect, keys = role
    if sorted(record['keywords'])!=sorted(('Devoid',*keys)):
        raise ValueError('printed abilities and keywords must agree')
    is_aura = effect=='aura'
    type_line = record['type_line']
    if (is_aura and type_line!='Enchantment — Aura') or (not is_aura and not type_line.startswith('Creature — ')):
        raise ValueError('reviewed exact permanent type required')
    price = cost_from_oracle(record)
    if not any(price[1:]):
        raise ValueError('printed colored cast cost required despite Devoid')
    return dict(effect=effect,price=price,power=None if is_aura else int(record['power']),
                toughness=None if is_aura else int(record['toughness']))


def variants(shape):
    return [f'{mode}_{seat}' for seat in (0,1) for mode in ('wrong_base',*MODES[shape['effect']])]


def spec(declaration,record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),shape)
            or not exact(profiles()['sources'].get(record['name'],{}).get('oracle'),record)):
        raise ValueError('exact full pinned Devoid source required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct reviewed Devoid variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle,rows):
    pins = profiles()
    if set(pins['witnesses'])!=WITNESSES or len(pins['sources'])!=len(RULES):
        raise ValueError('complete Devoid source and independent witnesses required')
    for name,pin in (pins['sources']|pins['witnesses']).items():
        row = rows[name]
        if (not exact(pin['oracle'],oracle[name]) or not exact(pin['registry'],row) or row['kind']!='card'
                or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in VANILLA):
            raise ValueError(f'moved Devoid source or independent witness: {name}')


class PaymentsBoard(DevoidBoard):
    def __init__(self,name,shape,mode,owner,rows,oracle):
        self.effect = shape['effect']
        self.life = [20,20]
        self.active = owner
        self.unbound = set()
        self.owner_destinations = {}
        super().__init__(name,shape,mode,owner,rows,oracle)

    def build(self):
        mode,effect = self.mode,self.effect
        if effect=='exile' and mode.startswith('return_'):
            self.setup[0]['zone'] = self.cards['source']['zone'] = 'graveyard'
            self.add('Forest','land',self.foe if mode=='return_no_land' else self.owner,tapped=True)
            return
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')
        if effect in ('blink','sacrifice','steal') and mode not in ('only_self','empty_foe','wrong_base'):
            actor = self.owner if mode in ('own_target','flight','haste') else self.foe
            self.add('Grizzly Bears','victim',actor,tapped=effect=='steal' or mode in ('flight','haste'))
            if mode=='foreign_owner':
                self.setup[-1].setdefault('state',[]).append('opponent_owned')
                self.owner_destinations['victim'] = 1-actor
        if mode=='paid' and effect in ('sacrifice','steal'):
            self.add('Wastes','producer',self.owner)
        if effect=='sacrifice' and mode=='cant_block':
            self.add('Wind Drake','attacker',self.foe)
        if effect=='block' and mode!='wrong_base':
            actor = self.owner if mode=='own_defender' else self.foe
            self.add('Grizzly Bears' if mode=='equal' else 'Giant Spider','blocker',actor)
            if mode in ('other_attacker','own_defender'):
                self.add('Hill Giant','attacker',1-actor)
        if effect=='ward':
            if mode not in ('foe_only','self_only','wrong_base'):
                self.add('Grizzly Bears','victim',self.owner)
            if mode=='foe_only':
                self.add('Grizzly Bears','foe_creature',self.foe)
            if mode not in ('self_only','foe_only','wrong_base'):
                self.add('Unsummon','enemy_spell',self.foe,'hand')
                self.add('Unsummon','friendly_spell',self.owner,'hand')
        if effect=='exile':
            if mode not in ('reach','creature_only','wrong_base'):
                name = {'exile_enchantment':'Fervor','exile_land':'Forest'}.get(mode,'Jalum Tome')
                self.add(name,'victim',self.foe,tapped=name=='Forest')
            if mode=='reach':
                self.add('Wind Drake','attacker',self.foe)
            elif mode=='creature_only':
                self.add('Grizzly Bears','wrong_type',self.foe)
        if effect=='aura' and mode!='no_creature':
            caster = self.owner if mode=='own_host' else self.foe
            name = 'Prodigal Sorcerer' if mode in ('noncombat','own_host') else 'Grizzly Bears'
            self.add(name,'host',caster)
            if mode=='cant_block':
                self.add('Hill Giant','attacker',self.owner)
            elif mode=='other_source':
                self.add('Prodigal Sorcerer','other',self.foe)

    def stat(self,ref,prop,value):
        self.emit('assert_card',ref=ref,property=prop,value=int(value))

    def pay(self,player,fee):
        after = [left-due for left,due in zip(self.pools[player],fee)]
        assert min(after)>=0
        self.pools[player] = after

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.observe_mana()
        quantities = Counter((card['player'],card['zone'],self.rows[card['name']]['csvid']) for card in self.cards.values())
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone in ('battlefield','hand','graveyard','exile','library'):
                self.emit('assert_zone',player=who,zone=zone,
                          value=sum(qty for (seat,location,_),qty in quantities.items() if (seat,location)==(who,zone)))
        for (who,zone,csvid),qty in sorted(quantities.items()):
            self.emit('assert_zone_multiplicity',player=who,zone=zone,index=csvid,value=qty)
        for ref,card in self.cards.items():
            if card['zone']!='battlefield':
                continue
            if ref in self.unbound:
                self.emit('assert_matching_permanents',player=card['player'],index=self.rows[card['name']]['csvid'],
                          property='tapped',value=int(card['tapped']))
                continue
            self.stat(ref,'in_play',1)
            self.stat(ref,'tapped',card['tapped'])
            stats = self.oracle[card['name']]
            if 'Creature' in types(stats):
                self.stat(ref,'power',int(stats['power']))
                self.stat(ref,'toughness',int(stats['toughness']))
                self.stat(ref,'damage',card['damage'])
        source = self.cards['source']
        if source['zone']=='battlefield':
            self.emit('assert_matching_permanents',player=source['player'],index=self.rows[source['name']]['csvid'],
                      property='color',value=0)

    def advance(self):
        if any(map(any,self.pools)):
            raise ValueError('real turn advancement requires empty fixture pools')
        self.emit('advance_turn')
        self.active = 1-self.active
        self.emit('untap_step',player=self.active)
        for card in self.cards.values():
            card['damage'] = 0
            if card['zone']=='battlefield' and card['player']==self.active:
                card['tapped'] = False
        self.emit('turn',player=self.active,value=0)

    def bind(self,ref):
        card = self.cards[ref]
        self.emit('bind_permanent_copy',ref=ref,player=card['player'],value=self.rows[card['name']]['csvid'],index=0)
        self.unbound.discard(ref)

    def finish(self,*,response=False):
        self.emit('resolve_response' if response else 'resolve')
        self.emit('assert_unused_choices',value=0)
        self.emit('refresh')

    def main_cast(self):
        mode = self.mode
        allowed = mode!='no_creature'
        if mode=='wrong_base':
            self.cast('source',funding=[sum(self.shape['price']),0,0,0,0,0],allowed=False)
            self.receipts()
            return False
        if not allowed:
            self.cast('source',allowed=False)
            self.receipts()
            return False
        if self.effect in ('sacrifice','steal','exile'):
            self.cast_with_trigger()
            return mode!='countered'
        if self.effect=='ward':
            self.cast_ward()
            return True
        self.cast('source',target='host' if self.effect=='aura' else None)
        self.finish()
        self.cards['source']['zone'] = 'battlefield'
        self.receipts()
        return True

    def cast_with_trigger(self):
        mode,effect = self.mode,self.effect
        cost = self.shape['price']
        budget = list(cost)
        fee_possible = effect in ('sacrifice','steal') and mode in ('paid','paid_attack','decline','empty_foe',
                                                                  'own_target','countered','replay')
        pay_optional = fee_possible and mode!='decline'
        if fee_possible:
            budget[0] += 2
        if mode=='colored_only':
            budget[2] += 2
        producer = 'producer' in self.cards
        if producer:
            budget[0] -= 1
        self.fund(self.owner,budget)
        if producer:
            self.emit('activate',ref='producer',value=1)
            self.emit('assert_stack',value=0)
            self.cards['producer']['tapped'] = True
            self.pools[self.owner][0] += 1
            self.observe_mana()
        if fee_possible:
            self.emit('choose_dialog',player=self.owner,value=int(not pay_optional))
        selected = 'victim' in self.cards
        if selected and (effect=='exile' or pay_optional and effect=='steal'):
            self.emit('choose_target',player=self.owner,ref='victim')
        if selected and pay_optional and effect=='sacrifice' and self.foe==0:
            self.emit('choose_target',player=self.foe,ref='victim')
        self.emit('cast',ref='source',value=1)
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=1)
        self.stat('source','in_hand',0)
        self.pay(self.owner,cost)
        if pay_optional:
            self.pay(self.owner,[2,0,0,0,0,0])
        self.observe_mana()
        self.cards['source']['zone'] = 'stack'
        if selected and (effect=='exile' or pay_optional):
            victim = self.cards['victim']
            if effect=='steal':
                victim['player'] = self.owner
                victim['tapped'] = False
                if mode!='own_target':
                    self.unbound.add('victim')
            else:
                victim['zone'] = 'exile' if effect=='exile' else 'graveyard'
        self.emit('announce_spell',ref='source')
        if mode=='countered':
            self.cast('counter',response=True)
            self.finish(response=True)
            self.cards['counter']['zone'] = self.cards['source']['zone'] = 'graveyard'
        else:
            self.finish()
            self.cards['source']['zone'] = 'battlefield'
        self.receipts()

    def ward_receipts(self,expected):
        self.emit('can_target_creature',ref='enemy_spell',other='victim',value=int(not expected))
        self.emit('can_target_creature',ref='friendly_spell',other='victim',value=1)

    def cast_ward(self):
        mode = self.mode
        has_target = 'victim' in self.cards
        if mode in ('response','offturn'):
            self.advance()
        response = mode=='response'
        if has_target:
            self.ward_receipts(False)
        if response:
            self.cast('enemy_spell',target='victim')
            self.emit('announce_spell',ref='enemy_spell')
        self.cast('source',response=response)
        if has_target:
            self.emit('choose_target',player=self.owner,ref='victim')
        self.finish(response=response)
        self.cards['source']['zone'] = 'battlefield'
        if response:
            self.cards['enemy_spell']['zone'] = 'graveyard'
        elif has_target:
            self.ward_receipts(True)
        self.receipts()
        if mode=='expiry':
            self.emit('cleanup')
            self.emit('refresh')
            self.ward_receipts(False)
            self.receipts()

    def colorless_ability(self):
        mode = self.mode
        if mode=='offturn':
            self.advance()
        allowed = mode not in ('only_self','colored_only','short')
        if mode=='colored_only':
            self.fund_colorless(0,3,generic_color=2)
        elif mode=='short':
            self.fund_colorless(1,1)
        else:
            self.fund_colorless(1,2)
        self.emit('can_activate',ref='source',value=int(allowed))
        if allowed and self.effect=='blink':
            self.emit('probe_activation_target',ref='source',other='source',index=-1,value=0)
            self.emit('probe_activation_target',ref='source',other='victim',index=-1,value=1)
            self.emit('choose_target',player=self.owner,ref='victim')
        self.emit('activate',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pay(self.owner,[3,0,0,0,0,0])
            self.observe_mana()
            self.finish()
            if self.effect=='blink':
                victim = self.cards['victim']
                victim['player'] = self.owner_destinations.get('victim',victim['player'])
                victim['tapped'] = True
                self.unbound.add('victim')
        self.receipts()

    def combat(self,attacker,blocker=None,*,blocked=False):
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.cards[attacker]['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocker:
            self.emit('block',ref=blocker,other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.finish()
        self.emit('end_combat')
        attack = self.cards[attacker]
        attack_stats = self.oracle[attack['name']]
        attack_power = int(attack_stats['power'])
        if blocked:
            block = self.cards[blocker]
            block_stats = self.oracle[block['name']]
            block_power = int(block_stats['power'])
            for entry,stats,received in ((attack,attack_stats,block_power),(block,block_stats,attack_power)):
                entry['damage'] += received
                if entry['damage']>=int(stats['toughness']):
                    entry['zone'] = 'graveyard'
        else:
            self.life[1-attack['player']] -= attack_power
        if self.effect=='aura' and attacker=='host':
            self.life[attack['player']] -= attack_power
        self.receipts()

    def block_effect(self):
        mode = self.mode
        if mode in ('own_defender','other_attacker'):
            if mode=='own_defender':
                self.advance()
            self.colorless_ability()
            self.combat('attacker','blocker',blocked=False)
            return
        if mode in ('colored_only','short'):
            self.colorless_ability()
            return
        self.advance()
        self.advance()
        self.colorless_ability()
        if mode=='expiry':
            self.advance()
            self.advance()
        self.combat('source','blocker',blocked=mode in ('equal','expiry'))

    def aura_combat_or_ping(self):
        mode = self.mode
        if mode=='cant_block':
            self.combat('attacker','host',blocked=False)
        if mode=='combat':
            self.advance()
            self.combat('host')
        if mode in ('noncombat','own_host','other_source','replay'):
            if mode=='replay':
                self.advance()
                self.combat('host')
                return
            ref = 'other' if mode=='other_source' else 'host'
            actor = self.cards[ref]['player']
            self.emit('choose_player',player=actor,value=1-actor)
            self.emit('activate',ref=ref,value=1)
            self.emit('assert_stack',value=1)
            self.finish()
            self.cards[ref]['tapped'] = True
            self.life[1-actor] -= 1
            if ref=='host':
                self.life[actor] -= 1
            self.receipts()

    def recover_world_breaker(self):
        mode = self.mode
        self.emit('prepare_graveyard_abilities')
        if mode=='return_offturn':
            # Switch to the real opposing turn before paying the instant-speed cost.
            self.advance()
        allowed = mode in ('return_paid','return_offturn')
        if mode=='return_wrong_color':
            self.fund_colorless(0,3,generic_color=2)
        elif mode=='return_short':
            self.fund_colorless(1,1)
        else:
            self.fund_colorless(1,2)
        self.emit('can_activate_graveyard',ref='source',value=int(allowed))
        if allowed:
            self.emit('choose_dialog',player=self.owner,value=1)
            if self.owner==0:
                self.emit('choose_target',player=0,ref='land')
        self.emit('activate_graveyard',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pay(self.owner,[3,0,0,0,0,0])
            self.cards['land']['zone'] = 'graveyard'
            source = self.rows[self.cards['source']['name']]['csvid']
            land = self.rows[self.cards['land']['name']]['csvid']
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=land,value=1)
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=source,value=1)
            self.emit('assert_zone_multiplicity',player=self.owner,zone='hand',index=source,value=0)
            self.observe_mana()
            self.finish()
            self.cards['source']['zone'] = 'hand'
            self.emit('bind_hand_copy',ref='returned',player=self.owner,index=0,value=source)
            self.stat('returned','in_hand',1)
        self.receipts()

    def play(self):
        self.emit('turn',player=self.owner,value=0)
        self.fund(self.foe,[0]*6)
        if (self.effect=='ward' and self.mode in ('response','offturn')) or self.mode=='return_offturn':
            # These fixtures advance before any source payment clears the initial owner pool.
            self.fund(self.owner,[0]*6)
        if self.mode.startswith('return_'):
            self.recover_world_breaker()
            return
        if not self.main_cast():
            return
        mode,effect = self.mode,self.effect
        if effect=='blink':
            self.colorless_ability()
        elif effect=='block':
            self.block_effect()
        elif effect=='steal' and mode in ('paid_attack','haste'):
            victim = mode=='paid_attack'
            if victim:
                self.bind('victim')
            self.combat('victim' if victim else 'source')
            if victim:
                self.emit('cleanup')
                self.emit('refresh')
                self.cards['victim']['player'] = self.foe
                self.unbound.add('victim')
                self.receipts()
        elif effect=='sacrifice' and mode=='flight':
            self.advance()
            self.advance()
            self.combat('source','victim',blocked=False)
        elif effect=='sacrifice' and mode=='cant_block':
            self.advance()
            self.combat('attacker','source',blocked=False)
        elif effect=='exile' and mode=='reach':
            self.advance()
            self.combat('attacker','source',blocked=True)
        elif effect=='aura':
            self.aura_combat_or_ping()


def expand(name,csvid,declaration,shape,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(shape)):
        raise ValueError('unbound Devoid source or variant')
    gaps = ['Multiplayer and direct spell-stack color observation.']
    if shape['effect'] in ('sacrifice','steal','exile'):
        gaps.append('Production embeds the cast rider in EVENT_CAST_SPELL, so no independent cast-trigger response priority is observable.')
    meta = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
    cases = []
    for variant in selected:
        mode,seat = variant.rsplit('_',1)
        board = PaymentsBoard(name,shape,mode,int(seat),rows,oracle)
        board.play()
        steps = board.steps
        if mode=='replay':
            steps = [step('capture'),*steps,step('record_state'),step('ai_roundtrip'),step('restore'),*steps,step('assert_recorded_state')]
        cases.append(dict(name=f'DevoidCastPaymentTargetRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=2172,setup=board.setup,steps=steps,_recipe=dict(meta,variant=variant)))
    return cases
