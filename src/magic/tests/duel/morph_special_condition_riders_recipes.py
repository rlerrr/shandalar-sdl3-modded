"""Paid Morph with distinct revealed activation and face-down boundaries."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_additional_cohort_riders_recipes import TargetScene
from morph_creature_recipes import FACE_DOWN, MORPH


FAMILY = 'morph_special_condition_riders'
PROFILE = Path(__file__).parent/'data/morph_special_condition_riders_wave8_profiles.json'
RULES = {
    ('{2}{R}, {T}: Destroy target nonbasic land.',): ('land_destroy', (), '{2}{R}'),
    ('Flying','{T}: This creature deals 1 damage to any target.'): ('ping', ('Flying',), ''),
    ("Defender (This creature can't attack.)",'{3}: Turn this creature face down.'): ('turn_down', ('Defender',), '{3}'),
    ('{T}: Add {C}.',): ('add_mana', (), ''),
}
COMMON = ('printed_effect','hidden','flip','flip_short','hidden_denied','replay')
EXTRA = {
    'land_destroy': ('basic_probe','fresh_denied'),
    'ping': ('walker_ping','flying_combat','hidden_combat'),
    'turn_down': ('defender','down_attack','reflect'),
    'add_mana': ('land_limit','mana_spend','offturn_flip'),
}
WITNESSES = {'Wasteland','Forest','Grizzly Bears','Jace Beleren','Sol Ring'}
VANILLA = {'Grizzly Bears'}


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    morph = MORPH.fullmatch(lines[-1])
    rule = RULES.get(tuple(lines[:-1]))
    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face') or not morph or not rule:
        raise ValueError('whole normal printed activation and paid Morph clause required')
    effect, keywords, ability = rule
    land = effect == 'add_mana'
    stats = (record.get('power'),record.get('toughness'))
    if (sorted(record['keywords']) != sorted(('Morph',*keywords)) or (land and record['type_line'] != 'Land')
            or (not land and (not record['type_line'].startswith('Creature — ')
                             or any(type(n) is not str or not n.isascii() or not n.isdecimal() for n in stats)))):
        raise ValueError('printed face characteristics do not match the complete activation')
    cast = [0]*6 if land else cost_from_oracle(record)
    morph_fee = cost_from_oracle({'mana_cost':morph[1]})
    if sum(morph_fee) <= 0:
        raise ValueError('nonzero actual face-up cost required')
    return dict(effect=effect, keywords=list(keywords), cast_mana=cast, flip_mana=morph_fee,
                ability_mana=cost_from_oracle({'mana_cost':ability}) if ability else [0]*6, power=int(stats[0] or 0), toughness=int(stats[1] or 0))


def variants(params):
    modes = COMMON+EXTRA[params['effect']]
    if params['effect']=='land_destroy':
        # A red pool sufficient for the printed activation also pays exactly
        # the alternate Morph action. Test short mana on the actual revealed face.
        modes = tuple('activation_short' if mode=='hidden_denied' else mode for mode in modes)
    return [f'{mode}_{owner}' for owner in (0,1) for mode in modes]


def spec(declaration,record):
    shape = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int or declaration['version'] != 2
            or not exact(profile()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'),shape)):
        raise ValueError('exact pinned activation and complete printed Morph required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(set(selected)) != len(selected) or set(selected)-set(variants(shape))):
        raise ValueError('distinct reviewed activated Morph variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES or len(pins['sources']) != len(RULES):
        raise ValueError('complete revealed-activation source and witness scope required')
    if pins['face_down']['csvid'] != FACE_DOWN or not exact(pins['face_down'],rows['Face-Down Creature']):
        raise ValueError('native face-down identity changed')
    for name,pin in (pins['sources']|pins['witnesses']).items():
        row = rows[name]
        if (not exact(oracle[name],pin['oracle']) or not exact(row,pin['registry']) or row['kind']!='card'
                or row['handler'] in (None,'NULL','0') or row['handler']=='card_dummy' and name not in VANILLA):
            raise ValueError(f'pinned activation source or witness changed: {name}')


class AbilityScene(TargetScene):
    def setup_cards(self):
        self.walker = None
        self.add(self.name,'source',self.owner,'hand')
        if self.effect == 'land_destroy':
            self.add('Wasteland','victim',self.foe,tapped=True)
            if self.mode == 'basic_probe':
                self.add('Forest','basic',self.foe,tapped=True)
        if self.effect == 'ping':
            if self.mode == 'walker_ping':
                self.add('Jace Beleren','victim',self.foe)
                self.walker = int(self.oracle['Jace Beleren']['loyalty'])
            else:
                self.add('Grizzly Bears','victim',self.foe)
        if self.effect == 'add_mana':
            if self.mode == 'land_limit':
                self.add('Forest','second_land',self.owner,'hand')
            if self.mode == 'mana_spend':
                self.add('Sol Ring','payment_spell',self.owner,'hand')

    def cast_chosen_face(self, faceup):
        if self.effect != 'add_mana':
            self.cast_both_available(faceup=faceup)
            return
        if faceup:
            self.emit('play_land',ref='source',value=1)
            self.prop('source','in_hand',0)
            self.emit('assert_stack',value=0)
        else:
            fee = [3,0,0,0,0,0]
            self.fund(self.owner, fee)
            self.emit('choose_dialog',player=self.owner,value=0)
            self.emit('cast',ref='source',value=1)
            self.emit('assert_unused_choices',value=0)
            self.prop('source','in_hand',0)
            self.emit('assert_stack',value=1)
            self.pay(self.owner, fee)
            self.hidden = True
            self.source_traits()
            self.pools()
            self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.receipts()

    def flip_plain(self, short=False):
        fee = self.model['flip_mana']
        budget = list(fee)
        if short:
            budget[next(index for index,n in enumerate(fee) if n)] -= 1
        self.fund(self.owner, budget)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(not short))
        self.emit('activate',ref='source',value=int(not short))
        self.emit('assert_unused_choices',value=0)
        if not short:
            self.hidden = False
            self.pay(self.owner, fee)
            self.resolve()
        self.receipts()

    def ready_on_own_turn(self):
        self.advance()
        self.advance()

    def ability(self, legal=True):
        fee = self.model['ability_mana']
        role = self.effect
        budget = [1,0,0,0,1,0] if self.mode=='activation_short' else fee
        self.fund(self.owner,budget)
        self.emit('can_activate',ref='source',value=int(legal))
        if self.mode == 'basic_probe':
            for ref,allowed in (('basic',0),('victim',1)):
                self.emit('probe_activation_target',ref='source',other=ref,index=-1,value=allowed)
        if legal and role in ('land_destroy','ping'):
            self.emit('choose_target',player=self.owner,ref='victim')
        self.emit('activate',ref='source',value=int(legal))
        if not legal:
            self.receipts()
            return
        self.pay(self.owner,fee)
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(role!='add_mana'))
        if role!='add_mana':
            self.resolve()
        if role in ('land_destroy','ping','add_mana'):
            self.entities['source']['tapped'] = True
        if role=='land_destroy':
            self.leave('victim','graveyard')
        elif role=='ping':
            if self.walker is not None:
                self.walker -= 1
            else:
                self.entities['victim']['damage'] = 1
        elif role=='turn_down':
            self.hidden = True
        else:
            self.mana[self.owner][0] += 1
        self.receipts()

    def combat_boundary(self):
        self.emit('begin_attackers')
        attacks = self.effect == 'ping' or self.hidden
        self.emit('attack',ref='source',value=int(attacks))
        self.entities['source']['tapped'] = attacks
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        if self.effect == 'ping':
            self.emit('block',ref='victim',other='source',value=int(self.hidden))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if self.effect == 'ping' and self.hidden:
            self.leave('source','graveyard')
            self.leave('victim','graveyard')
        elif attacks:
            self.life[self.foe] -= self.stats('source')[0]
        self.receipts()

    def spend_output(self):
        self.emit('cast',ref='payment_spell',value=1)
        self.pay(self.owner,[1,0,0,0,0,0])
        self.emit('assert_stack',value=1)
        self.pools()
        self.resolve()
        self.entities['payment_spell']['zone'] = 'battlefield'
        self.receipts()

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')
        mode = self.mode
        cast_up = mode in ('printed_effect','fresh_denied','defender','down_attack','reflect','land_limit')
        self.cast_chosen_face(cast_up)
        if mode=='hidden':
            return
        if mode=='land_limit':
            self.emit('play_land',ref='second_land',value=0)
            self.prop('second_land','in_hand',1)
            self.ability()
            return
        if mode=='offturn_flip':
            self.advance()
        need_ready = (self.effect in ('land_destroy','ping') and mode not in ('fresh_denied','flip_short')
                      or mode in ('defender','down_attack'))
        if need_ready:
            self.ready_on_own_turn()
        if mode=='hidden_denied':
            self.ability(legal=False)
            return
        if mode=='flip_short':
            self.flip_plain(short=True)
            return
        if not cast_up and mode!='hidden_combat':
            self.flip_plain()
        if mode in ('flying_combat','hidden_combat','defender'):
            self.combat_boundary()
            return
        self.ability(legal=mode not in ('fresh_denied','activation_short'))
        if mode=='down_attack':
            self.combat_boundary()
        if mode=='reflect':
            self.flip_plain()
        if mode=='mana_spend':
            self.spend_output()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not selected or set(selected)-set(variants(model)):
        raise ValueError('unbound revealed activation or variant')
    metadata = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                    covered_clauses=declaration['oracle_text'].splitlines(),
                    untested_components=['Multiplayer and face-down concealment from an external human UI'])
    cases = []
    for variant in selected:
        mode,owner = variant.rsplit('_',1)
        scene = AbilityScene(name,model,mode,int(owner),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            tail = steps[at:]
            steps = [*steps[:at],step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                     step('restore'),*tail,step('assert_recorded_state')]
        cases.append(dict(name=f'MorphSpecialConditionRiderRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=2047,setup=scene.setup,steps=steps,_recipe=dict(metadata,variant=variant)))
    return cases
