"""Equipment with a color-conditional upkeep counter and a second effect."""
from collections import Counter
import copy
from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact

FAMILY = 'ring_equipment_upkeep'
PROFILE = Path(__file__).parent / 'data/ring_equipment_upkeep_wave8_profiles.json'
UPKEEP = r"At the beginning of your upkeep, put a \+1/\+1 counter on equipped creature if it's (black|blue|green|red|white)\."
ENTRY_LIFE = r'When this creature enters, you gain ([1-9][0-9]*) life\.'
EQUIP = 'Equip {1} ({1}: Attach to target creature you control. Equip only as a sorcery.)'
EFFECTS = {
    "{2}: Equipped creature gains hexproof until end of turn. (It can't be the target of spells or abilities your opponents control.)": 'hexproof',
    "Equipped creature has trample. (It can deal excess combat damage to the player or planeswalker it's attacking.)": 'trample',
    "Equipped creature has vigilance. (Attacking doesn't cause it to tap.)": 'vigilance',
    'Equipped creature has haste. (It can attack and {T} no matter when it came under your control.)': 'haste',
    "{2}: Regenerate equipped creature. (The next time that creature would be destroyed this turn, instead tap it, remove it from combat, and heal all damage on it.)": 'regenerate',
}
SYMBOL = {'black':'B', 'blue':'U', 'green':'G', 'red':'R', 'white':'W'}
HOSTS = {'black':'Child of Night', 'blue':'Merfolk Looter', 'green':'Rootwalla',
         'red':'Goblin Fireslinger', 'white':'Lone Missionary'}
COMMON = ('growth_twice', 'wrong_color', 'unattached', 'move_to_match', 'move_to_nonmatch',
          'equip_unfunded', 'equip_wrong_controller', 'equip_wrong_type', 'equip_offturn',
          'equip_target_leaves', 'equipment_removed', 'pending_equip_replay')
EFFECT_MODES = {
    'hexproof': ('hexproof_offturn', 'hexproof_unfunded', 'hexproof_moved'),
    'trample': ('trample_combat', 'trample_removed'),
    'vigilance': ('vigilance_attack', 'vigilance_removed', 'multicolor'),
    'haste': ('haste_fresh_attack', 'haste_fresh_tap', 'haste_removed', 'multicolor'),
    'regenerate': ('regenerate_window', 'regenerate_unfunded', 'regenerate_proactive'),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    lines = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or
            record.get('type_line') != 'Artifact — Equipment' or record.get('mana_cost') != '{2}' or
            len(lines) != 3 or lines[0] not in EFFECTS or lines[2] != EQUIP):
        raise ValueError('whole normal Equipment, resolution effect and exact printed Equip required')
    effect = EFFECTS[lines[0]]
    upkeep = re.fullmatch(UPKEEP, lines[1])
    keywords = {'Equip', 'Regenerate', 'Heal'} if effect=='regenerate' else {'Equip'}
    if not upkeep or set(record['keywords']) != keywords:
        raise ValueError('complete upkeep color and structured keywords required')
    return dict(schema=1, color=upkeep[1], effect=effect, equip=1,
                active_cost=2 if effect in ('hexproof','regenerate') else 0)


def variants(model):
    return [f'{mode}_{owner}' for owner in (0,1) for mode in COMMON + EFFECT_MODES[model['effect']]]


def spec(declaration, record):
    from recipes import cost_from_oracle

    model = parameters(record)
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or
            declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text'] or
            not exact(declaration.get('params'), model)):
        raise ValueError('typed whole Ring declaration required')
    selected = declaration.get('variants')
    if (not isinstance(selected, list) or not selected or any(not isinstance(v,str) for v in selected) or
            len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('explicit unique supported Ring variants required')
    return dict(model, family=FAMILY, mana=cost_from_oracle(record))


def validate_witnesses(oracle, registry):
    data = pins()
    for group in ('sources','witnesses'):
        for name, pin in data[group].items():
            card = oracle.get(name,{})
            row = registry.get(name,{})
            if (not exact(card,pin['oracle']) or not exact(row,pin['registry']) or card.get('layout') != 'normal' or
                    row.get('kind') != 'card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'implemented normal Ring {group} identity changed: {name}')


class Scenario:
    def __init__(self, name, owner, model, registry, oracle, mode):
        self.name, self.owner, self.enemy = name, owner, 1-owner
        self.model, self.registry, self.oracle, self.mode = model, registry, oracle, mode
        self.setup, self.steps, self.entities = [], [], {}
        self.mana = [[0]*6 for _ in range(2)]
        self.life = [20,20]
        self.active, self.equipped = owner, None
        self.snapshot = None

    def emit(self, op, **values):
        from recipes import step

        self.steps.append(step(op, **values))

    def add(self, name, ref, player, zone='hand'):
        from recipes import card

        self.setup.append(card(name,ref,player,zone))
        self.entities[ref] = dict(name=name,player=player,zone=zone,counters=0,tapped=0,damage=0)
        # Battlefield setup goes through the real enter-the-battlefield path.
        gain = re.fullmatch(ENTRY_LIFE,self.oracle[name]['oracle_text'])
        if zone=='battlefield' and gain:
            self.life[player] += int(gain[1])

    def fund(self, player, amounts):
        if any(self.mana[player]):
            raise ValueError('spend mana before funding another real action')
        self.mana[player] = list(amounts)
        for color, amount in enumerate(amounts):
            if amount:
                self.emit('set_mana',player=player,color=color,value=amount)
        self.emit('refresh')

    def fund_generic(self, player, amount):
        self.fund(player, [amount,0,0,0,0,0])

    def pay(self, player, amounts):
        self.mana[player] = [before-cost for before,cost in zip(self.mana[player],amounts)]
        if min(self.mana[player]) < 0:
            raise ValueError('unfunded successful Ring action')

    def property(self, ref, prop, value, required=False):
        op = 'require_card' if required else 'assert_card'
        self.emit(op,ref=ref,property=prop,value=value)

    def creature(self, ref):
        return 'Creature' in self.oracle[self.entities[ref]['name']]['type_line'].split(' — ')[0]

    def observe_host(self, ref):
        entity = self.entities[ref]
        self.property(ref,'in_play',int(entity['zone']=='battlefield'))
        if entity['zone'] != 'battlefield' or not self.creature(ref):
            return
        record = self.oracle[entity['name']]
        amount = entity['counters']
        self.emit('assert_counter',ref=ref,counter_kind='plus1',value=amount)
        for prop in ('power','toughness'):
            self.property(ref,prop,int(record[prop])+amount)
        for prop in ('tapped','damage'):
            self.property(ref,prop,entity[prop])

    def choices(self):
        self.emit('assert_choices_consumed')

    def pools(self):
        for player, pool in enumerate(self.mana):
            for color, amount in enumerate(pool):
                self.emit('assert_mana',player=player,color=color,value=amount)

    def resolve(self):
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_stack',value=0)
        self.choices()

    def cast(self, ref, target=None, stack=1):
        from recipes import cost_from_oracle

        entity = self.entities[ref]
        player = entity['player']
        amount = cost_from_oracle(self.oracle[entity['name']])
        self.fund(player,amount)
        if target:
            self.emit('choose_target',player=player,ref=target)
        self.emit('cast',ref=ref,value=1)
        self.emit('assert_stack',value=stack)
        self.choices()
        self.pay(player,amount)
        self.pools()

    def cast_permanent(self, ref):
        self.cast(ref)
        self.resolve()
        self.entities[ref]['zone'] = 'battlefield'
        self.property(ref,'in_play',1,required=True)

    def equip(self, ref='host', legal=True, funded=True, pending=False):
        amount = self.model['equip']
        self.fund_generic(self.owner,amount if funded else 0)
        self.emit('require_can_activate',ref='source',value=int(legal))
        if legal:
            # The M13 dialog omits illegal actions and has no single-choice
            # auto-selection. With exactly one mana, Equip is its first row.
            self.emit('choose_dialog',player=self.owner,value=0)
            self.emit('choose_target',player=self.owner,ref=ref)
        self.emit('activate',ref='source',value=int(legal))
        self.emit('assert_stack',value=int(legal))
        self.choices()
        if legal:
            self.pay(self.owner,[amount,0,0,0,0,0])
        self.pools()
        if legal and not pending:
            self.finish_equip(ref)

    def finish_equip(self, ref):
        self.resolve()
        self.equipped = ref

    def upkeep(self):
        if any(map(any,self.mana)):
            raise ValueError('upkeep fixtures must enter with empty mana pools')
        self.emit('advance_turn')
        self.active = 1-self.active
        self.emit('untap_step',player=self.active)
        self.emit('upkeep_step',player=self.active)
        self.choices()
        self.emit('assert_stack',value=0)
        for entity in self.entities.values():
            if entity['player'] == self.active:
                entity['tapped'] = 0
            entity['damage'] = 0
        if self.active == self.owner and self.equipped:
            subject = self.entities[self.equipped]
            if SYMBOL[self.model['color']] in self.oracle[subject['name']]['colors']:
                subject['counters'] += 1
        for ref in ('host','other_host'):
            if ref in self.entities:
                self.observe_host(ref)

    def remove(self, ref, victim, destination, stack=1):
        self.cast(ref,victim,stack=stack)
        self.resolve()
        self.entities[ref]['zone'] = 'graveyard'
        self.entities[victim]['zone'] = destination
        self.property(victim,'in_play',0,required=True)
        if victim in ('source',self.equipped):
            self.equipped = None

    def select_turn(self, player):
        self.emit('turn',player=player,value=0)
        self.active = player

    def active_ability(self, available, funded=True):
        amount = self.model['active_cost']
        self.fund_generic(self.owner,amount if funded else amount-1)
        self.emit('require_can_activate',ref='source',value=int(available))
        if available:
            # Off the Ring controller's turn, the non-Equip effect is the
            # sole offered mode and occupies row zero if legally available.
            self.emit('choose_dialog',player=self.owner,value=0)
        self.emit('activate',ref='source',value=int(available))
        self.emit('assert_stack',value=int(available))
        self.choices()
        if available:
            self.pay(self.owner,[amount,0,0,0,0,0])
            self.resolve()
        self.pools()

    def hexproof(self):
        mode = self.mode
        self.select_turn(self.enemy)
        target = self.equipped
        self.active_ability(mode!='hexproof_unfunded',funded=mode!='hexproof_unfunded')
        self.emit('can_target_creature',ref='enemy_probe',other=target,value=int(mode=='hexproof_unfunded'))
        self.emit('can_target_creature',ref='own_probe',other=target,value=1)
        if mode=='hexproof_moved':
            self.emit('can_target_creature',ref='enemy_probe',other='host',value=1)
        if mode=='hexproof_offturn':
            self.emit('cleanup')
            self.emit('refresh')
            self.emit('can_target_creature',ref='enemy_probe',other=target,value=1)

    def combat(self, attack=True, blocking=False, trample=False, vigilant=False):
        host = self.entities['host']
        self.emit('begin_attackers')
        self.emit('attack',ref='host',value=int(attack))
        self.property('host','attacking',int(attack))
        host['tapped'] = int(attack and not vigilant)
        self.property('host','tapped',host['tapped'])
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if blocking:
            self.emit('block',ref='blocker',other='host',value=1)
            self.property('blocker','blocking_count',1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power = int(self.oracle[host['name']]['power'])
        if blocking:
            blocker = self.entities['blocker']
            record = self.oracle[blocker['name']]
            host['damage'] = int(record['power'])
            blocker['zone'] = 'graveyard'
            self.life[self.enemy] -= max(0,power-int(record['toughness'])) if trample else 0
        elif attack:
            self.life[self.enemy] -= power

    def regeneration(self):
        mode = self.mode
        if mode=='regenerate_window':
            self.cast('mark_damage','host')
            self.resolve()
            self.entities['mark_damage']['zone'] = 'graveyard'
            self.entities['host']['damage'] = 2
            self.observe_host('host')
            self.emit('begin_attackers')
            self.emit('attack',ref='host',value=1)
            self.property('host','attacking',1)
            self.emit('finish_attackers')
            self.emit('begin_blockers')
            self.fund_generic(self.owner,self.model['active_cost'])
        else:
            self.select_turn(self.enemy)
            if mode=='regenerate_proactive':
                self.active_ability(True)
        self.cast('destroy_host','host')
        if mode=='regenerate_window':
            self.emit('choose_regeneration',ref='source')
            self.emit('choose_dialog',player=self.owner,value=0)
            self.pay(self.owner,[self.model['active_cost'],0,0,0,0,0])
        self.resolve()
        self.entities['destroy_host']['zone'] = 'graveyard'
        if mode=='regenerate_unfunded':
            self.entities['host']['zone'] = 'graveyard'
            self.equipped = None
        else:
            self.entities['host']['tapped'] = 1
            self.entities['host']['damage'] = 0
            self.observe_host('host')
            self.property('host','attacking',0)
            self.pools()
            self.remove('second_destruction','host','graveyard')
        if mode=='regenerate_window':
            self.emit('advance_combat')
            self.emit('combat_damage')
            self.resolve()
            self.emit('end_combat')

    def initial_cards(self):
        mode = self.mode
        matching = HOSTS[self.model['color']]
        contrary = 'Child of Night' if self.model['color']=='green' else 'Elvish Mystic'
        host = contrary if mode in ('wrong_color','move_to_match','haste_fresh_tap','hexproof_moved') else matching
        if mode=='multicolor':
            host = 'Boros Recruit'
        if mode=='equip_wrong_type':
            host = 'Darksteel Relic'
        if mode=='vigilance_attack' or mode=='vigilance_removed':
            host = 'Lone Missionary'
        if mode=='regenerate_window':
            host = 'Giant Scorpion'
        self.add(self.name,'source',self.owner)
        who = self.enemy if mode=='equip_wrong_controller' else self.owner
        fresh = mode in ('haste_fresh_attack','haste_fresh_tap','haste_removed')
        self.add(host,'host',who,'hand' if fresh else 'battlefield')
        if mode in ('move_to_match','move_to_nonmatch','hexproof_moved'):
            self.add(contrary if mode=='move_to_nonmatch' else matching,'other_host',self.owner,'battlefield')
        if mode=='equip_target_leaves':
            self.add('Unsummon','bounce',self.enemy)
        if mode in ('equipment_removed','trample_removed','vigilance_removed','haste_removed'):
            self.add('Shatter','shatter',self.enemy)
        if mode.startswith('trample_'):
            self.add('Elvish Mystic','blocker',self.enemy,'battlefield')
        if mode.startswith('hexproof_'):
            self.add('Unsummon','own_probe',self.owner)
            self.add('Unsummon','enemy_probe',self.enemy)
        if mode.startswith('regenerate_'):
            self.add('Murder','destroy_host',self.enemy)
            if mode=='regenerate_window':
                self.add('Shock','mark_damage',self.enemy)
                self.add('Murder','second_destruction',self.enemy)
            elif mode=='regenerate_proactive':
                self.add('Shock','second_destruction',self.enemy)

    def play(self):
        mode = self.mode
        self.initial_cards()
        self.select_turn(self.owner)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.cast_permanent('source')
        if self.entities['host']['zone']=='hand':
            self.cast_permanent('host')
            self.property('host','sick',1)
        if mode=='unattached':
            return
        if mode in ('equip_unfunded','equip_wrong_controller','equip_wrong_type','equip_offturn'):
            if mode=='equip_offturn':
                self.select_turn(self.enemy)
            self.equip(legal=False,funded=mode!='equip_unfunded')
            return
        pending = mode in ('equip_target_leaves','pending_equip_replay')
        self.equip(pending=pending)
        if mode=='equip_target_leaves':
            self.remove('bounce','host','hand',stack=2)
        if mode=='pending_equip_replay':
            self.snapshot = len(self.steps)
            self.finish_equip('host')
        if mode in ('move_to_match','move_to_nonmatch','hexproof_moved'):
            self.equip('other_host')
        if mode in ('equipment_removed','trample_removed','vigilance_removed','haste_removed'):
            self.remove('shatter','source','graveyard')
        if mode.startswith('hexproof_'):
            self.hexproof()
        if mode.startswith('trample_'):
            self.combat(blocking=True,trample=mode=='trample_combat')
        if mode.startswith('vigilance_'):
            self.combat(vigilant=mode=='vigilance_attack')
        if mode in ('haste_fresh_attack','haste_removed'):
            self.combat(attack=mode=='haste_fresh_attack')
        if mode=='haste_fresh_tap':
            self.emit('require_can_activate',ref='host',value=1)
            self.emit('activate',ref='host',value=1)
            self.entities['host']['tapped'] = 1
            self.mana[self.owner][3] += 1
        if mode.startswith('regenerate_'):
            self.regeneration()

    def receipts(self):
        self.emit('assert_stack',value=0)
        self.choices()
        self.pools()
        for ref in self.entities:
            self.observe_host(ref)
        for player in (0,1):
            self.emit('assert_life',player=player,value=self.life[player])
            for zone in ('hand','battlefield','graveyard','library','exile'):
                counts = Counter(e['name'] for e in self.entities.values()
                                 if e['player']==player and e['zone']==zone)
                self.emit('assert_zone',player=player,zone=zone,value=sum(counts.values()))
                for name, amount in counts.items():
                    self.emit('assert_zone_multiplicity',player=player,zone=zone,index=self.registry[name]['csvid'],value=amount)

    def run(self):
        self.play()
        if self.mode in ('growth_twice','wrong_color','unattached','move_to_match','move_to_nonmatch',
                         'equip_target_leaves','equipment_removed','pending_equip_replay','multicolor'):
            for _ in range(4 if self.mode=='growth_twice' else 2):
                self.upkeep()
            self.emit('cleanup')
            self.emit('refresh')
        self.receipts()
        if self.snapshot is not None:
            from recipes import step

            tail = self.steps[self.snapshot:]
            self.steps = (self.steps[:self.snapshot]+[step('capture')]+tail+
                          [step('record_state'),step('ai_roundtrip'),step('restore')]+
                          copy.deepcopy(tail)+[step('assert_recorded_state')])


def expand(name, csvid, declaration, model, selected, registry, oracle):
    if pins()['sources'].get(name,{}).get('registry',{}).get('csvid') != csvid:
        raise ValueError('unreserved Ring source')
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = Scenario(name,int(owner),model,registry,oracle,mode)
        scene.run()
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],untested_components=[
                            'Changes to color or control in response to a pending upkeep trigger',
                            'Multiple Equipment and combat damage assignment to planeswalkers'])
        cases.append(dict(name=f'RingEquipmentUpkeepRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=scene.setup,steps=scene.steps,_recipe=metadata))
    return cases
