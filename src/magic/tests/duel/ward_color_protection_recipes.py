"""Paid Wards and the scope of their Aura-specific protection exemption."""
from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact


FAMILY = 'ward_color_protection'
PROFILE = Path(__file__).parent / 'data/ward_color_protection_wave8_profiles.json'
COLORS = dict(zip(('black','blue','green','red','white'),range(1,6)))
PROTECTION = r"Enchanted creature has protection from (black|blue|green|red|white)\. This effect doesn't remove this Aura\."
PROBES = dict(zip(COLORS,('Murder','Unsummon','Giant Growth','Shock','Swords to Plowshares')))
AURAS = dict(zip(COLORS,('Unholy Strength','Flight','Feral Invocation','Goblin War Paint','Holy Strength')))
COMBATANTS = dict(zip(COLORS,('Giant Scorpion','Merfolk Looter','Rootwalla','Goblin Tinkerer','Veteran Swordsmith')))
BASE = ('own_attachment','foreign_attachment','no_creature','unfunded','colored_targeting','same_color_aura',
        'matching_block_denied','other_color_block','colored_combat_damage','cleanup','ward_bounce_recast',
        'host_leaves','pending_replay','host_lost_pending')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text','').splitlines()
    if (record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line')!='Enchantment — Aura' or record.get('mana_cost')!='{W}' or
            record.get('colors')!=['W'] or record.get('keywords')!=['Enchant'] or
            len(lines)!=2 or lines[0]!='Enchant creature'):
        raise ValueError('whole normal white Enchant creature Aura required')
    effect = re.fullmatch(PROTECTION,lines[1])
    if not effect:
        raise ValueError('exact permanent color protection and this-effect-only exemption required')
    return dict(schema=1,color=effect[1],exempt_self=True)


def variants(model):
    color = model['color']
    modes = BASE + (('independent_protection',) if color=='white' else ('recolored_exemption',))
    if color in ('red','white'):
        modes += ('multicolor_block_denied',)
    return [f'{mode}_{player}' for player in (0,1) for mode in modes]


def spec(declaration, record):
    model = parameters(record)
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or
            declaration['version']!=2 or declaration.get('oracle_text')!=record['oracle_text'] or
            not exact(declaration.get('params'),model)):
        raise ValueError('typed whole Ward declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected,list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('explicit distinct valid Ward variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, registry):
    for group in ('sources','witnesses'):
        for name, pin in pins()[group].items():
            record, row = oracle.get(name,{}), registry.get(name,{})
            if (not exact(record,pin['oracle']) or not exact(row,pin['registry']) or record.get('layout')!='normal' or
                    row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Ward {group} identity changed: {name}')


class Scene:
    def __init__(self, name, owner, mode, model, registry, oracle):
        self.name, self.owner, self.enemy, self.mode = name, owner, 1-owner, mode
        self.color, self.registry, self.oracle = model['color'], registry, oracle
        self.setup, self.steps, self.entities = [], [], {}
        self.mana = [[0]*6 for _ in (0,1)]
        self.life = [20,20]
        self.snapshot = None

    def emit(self, op, **fields):
        from recipes import step

        self.steps.append(step(op,**fields))

    def add(self, ref, printing, player=None, zone='hand'):
        from recipes import card

        player = self.owner if player is None else player
        self.setup.append(card(printing,ref,player,zone))
        record = self.oracle[printing]
        self.entities[ref] = dict(name=printing,player=player,zone=zone,damage=0,tapped=False,
                                  power=int(record.get('power') or 0),toughness=int(record.get('toughness') or 0))

    def prop(self, ref, prop, value, required=False):
        self.emit('require_card' if required else 'assert_card',ref=ref,property=prop,value=int(value))

    def observe_creature(self, ref):
        item = self.entities[ref]
        for prop in ('power','toughness','damage','tapped'):
            self.prop(ref,prop,item[prop])

    def pools(self):
        for player, amounts in enumerate(self.mana):
            for color, value in enumerate(amounts):
                self.emit('assert_mana',player=player,color=color,value=value)

    def fund(self, player, amount):
        if any(self.mana[player]):
            raise ValueError('spend the old pool before funding the next action')
        self.mana[player] = list(amount)
        for color, value in enumerate(amount):
            if value:
                self.emit('set_mana',player=player,color=color,value=value)
        self.emit('refresh')

    def paid(self, player):
        self.mana[player] = [0]*6
        self.pools()

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=0)

    def cast(self, ref, target, *, stack=1):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['player']
        self.fund(player,cost_from_oracle(self.oracle[entity['name']]))
        self.emit('require_can_cast',ref=ref,value=1)
        self.emit('choose_target',player=player,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=stack)
        self.paid(player)

    def into_play(self, ref):
        self.resolve()
        self.entities[ref]['zone'] = 'battlefield'
        self.prop(ref,'in_play',1,required=True)

    def use(self, ref, target, *, choose_color=None, payment=0):
        item = self.entities[ref]
        player = item['player']
        if payment:
            self.fund(player,[payment,0,0,0,0,0])
        self.emit('require_can_activate',ref=ref,value=1)
        self.emit('choose_target',player=player,ref=target)
        self.emit('activate',ref=ref,value=1)
        self.emit('assert_choices_consumed')
        if choose_color is not None:
            self.prop(ref,'tapped',1)
            self.emit('choose_color',player=player,value=COLORS[choose_color])
            item['tapped'] = True
        self.emit('assert_stack',value=1)
        self.paid(player)
        self.resolve()

    def check_target(self, probe, target, legal):
        self.emit('can_target_creature',ref=probe,other=target,value=int(legal))

    def attached_checks(self):
        self.prop('source','in_play',1)
        self.check_target('probe','host',False)

    def setup_cards(self):
        mode = self.mode
        self.add('source',self.name)
        if mode=='no_creature':
            self.add('noncreature','Darksteel Relic',zone='battlefield')
            return
        if mode=='unfunded':
            self.add('host','Rootwalla',zone='battlefield')
            return
        self.add('host','Rootwalla',self.enemy if mode=='foreign_attachment' else self.owner,'battlefield')
        self.add('probe',PROBES[self.color],self.enemy)
        if mode in ('colored_targeting','ward_bounce_recast'):
            self.add('witness','Manakin',self.owner,'battlefield')
        if mode=='colored_targeting':
            self.add('own_probe',PROBES[self.color])
            self.add('other_probe','Giant Growth' if self.color=='blue' else 'Twiddle',self.enemy)
        if mode in ('same_color_aura','recolored_exemption'):
            self.add('ordinary_aura',AURAS[self.color] if mode=='same_color_aura' else 'Holy Strength')
        if mode=='recolored_exemption':
            self.add('ward_lens','Distorting Lens',zone='battlefield')
            self.add('ordinary_lens','Distorting Lens',zone='battlefield')
        if mode=='independent_protection':
            self.add('sword','Sword of War and Peace',zone='battlefield')
        if mode in ('matching_block_denied','other_color_block','colored_combat_damage','multicolor_block_denied'):
            name = COMBATANTS[self.color]
            if mode=='other_color_block':
                name = COMBATANTS['red' if self.color=='blue' else 'blue']
            if mode=='multicolor_block_denied':
                name = 'Boros Recruit'
            self.add('combatant',name,self.enemy,'battlefield')
        if mode in ('host_leaves','host_lost_pending'):
            self.add('removal','Murder' if mode=='host_leaves' and self.color=='blue' else 'Unsummon',self.enemy)
        if mode=='ward_bounce_recast':
            self.add('bounce','Boomerang',self.enemy)

    def combat(self):
        defending = self.mode=='colored_combat_damage'
        attack, block = ('combatant','host') if defending else ('host','combatant')
        attacker, blocker = self.entities[attack], self.entities[block]
        turn = attacker['player']
        legal = defending or self.mode=='other_color_block'
        self.emit('turn',player=turn,value=0)
        self.emit('begin_attackers')
        self.emit('attack',ref=attack,value=1)
        self.prop(attack,'attacking',1)
        attacker['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref=block,other=attack,value=int(legal))
        self.prop(block,'blocking_count',int(legal))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if not legal:
            self.life[1-turn] -= attacker['power']
            return
        if defending:
            attacker['damage'] = blocker['power']
            if attacker['damage']>=attacker['toughness']:
                attacker['zone'] = 'graveyard'
        else:
            attacker['damage'] = blocker['power']
            blocker['zone'] = 'graveyard'

    def remove_host(self, pending=False):
        self.cast('removal','host',stack=2 if pending else 1)
        self.resolve()
        self.entities['source']['zone'] = 'graveyard'
        removal = self.entities['removal']
        self.entities['host']['zone'] = 'hand' if removal['name']=='Unsummon' else 'graveyard'
        removal['zone'] = 'graveyard'

    def play(self):
        mode = self.mode
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')
        if mode in ('no_creature','unfunded'):
            if mode=='no_creature':
                self.fund(self.owner,[0,0,0,0,0,1])
                self.emit('require_can_cast',ref='source',value=0)
            self.emit('cast',ref='source',value=0)
            return
        if mode in ('same_color_aura','recolored_exemption'):
            self.cast('ordinary_aura','host')
            self.into_play('ordinary_aura')
        self.check_target('probe','host',True)
        self.cast('source','host')
        self.check_target('probe','host',True)
        if mode=='pending_replay':
            self.snapshot = len(self.steps)
        if mode=='host_lost_pending':
            self.remove_host(pending=True)
            return
        self.into_play('source')
        self.attached_checks()
        if mode=='same_color_aura':
            self.entities['ordinary_aura']['zone'] = 'graveyard'
        if mode=='recolored_exemption':
            self.prop('ordinary_aura','in_play',1)
            self.prop('host','power',3)
            self.prop('host','toughness',4)
            self.use('ward_lens','source',choose_color=self.color)
            self.attached_checks()
            self.use('ordinary_lens','ordinary_aura',choose_color=self.color)
            self.attached_checks()
            self.entities['ordinary_aura']['zone'] = 'graveyard'
        if mode=='independent_protection':
            self.use('sword','host',payment=2)
            self.entities['source']['zone'] = 'graveyard'
            host = self.entities['host']
            host['power'] += 2
            host['toughness'] += 2
            self.check_target('probe','host',False)
        if mode=='cleanup' or mode=='pending_replay':
            self.emit('cleanup')
            self.emit('refresh')
            self.attached_checks()
        if mode=='colored_targeting':
            self.check_target('own_probe','host',False)
            for probe in ('probe','own_probe'):
                self.check_target(probe,'witness',True)
            self.check_target('other_probe','host',True)
            self.cast('other_probe','host')
            item = self.entities['other_probe']
            if item['name']=='Twiddle':
                self.emit('choose_dialog',player=self.enemy,value=0)
                self.entities['host']['tapped'] = True
            else:
                for stat in ('power','toughness'):
                    self.entities['host'][stat] += 3
            self.resolve()
            item['zone'] = 'graveyard'
            self.attached_checks()
        if mode in ('matching_block_denied','other_color_block','colored_combat_damage','multicolor_block_denied'):
            self.combat()
        if mode=='host_leaves':
            self.remove_host()
        if mode=='ward_bounce_recast':
            self.cast('bounce','source')
            self.resolve()
            self.entities['bounce']['zone'] = 'graveyard'
            csvid = self.registry[self.name]['csvid']
            self.emit('bind_hand',ref='source',player=self.owner,value=csvid)
            self.prop('source','in_hand',1,required=True)
            self.check_target('probe','host',True)
            self.cast('source','witness')
            self.into_play('source')
            self.check_target('probe','witness',False)
            self.check_target('probe','host',True)

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.pools()
        for ref, item in self.entities.items():
            alive = item['zone']=='battlefield'
            self.prop(ref,'in_play',alive)
            if alive and 'Creature' in self.oracle[item['name']]['type_line'].split(' — ')[0]:
                self.observe_creature(ref)
        for player, life in enumerate(self.life):
            self.emit('assert_life',player=player,value=life)
            for zone in ('hand','battlefield','graveyard','exile','library'):
                counts = Counter(x['name'] for x in self.entities.values() if x['player']==player and x['zone']==zone)
                self.emit('assert_zone',player=player,zone=zone,value=sum(counts.values()))
                for printing, value in counts.items():
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.registry[printing]['csvid'],value=value)


def expand(name, csvid, declaration, model, selected, registry, oracle):
    from recipes import step

    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid')!=csvid:
        raise ValueError('unreserved Ward source')
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
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'Multiple overlapping protection grants on the same recolored nonwhite Ward',
                            'Colored Equipment detachment and multiplayer'])
        cases.append(dict(name=f'WardColorProtectionRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
