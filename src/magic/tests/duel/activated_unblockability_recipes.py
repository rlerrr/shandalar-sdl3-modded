"""Paid evasion proven by real blocker declarations, costs and turn transitions."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene


FAMILY = 'activated_unblockability'
PROFILE = Path(__file__).parent / 'data/activated_unblockability_wave8_profiles.json'
TARGET = re.compile(r'Target creature( with power ([1-9][0-9]*) or less)? (gains haste until end of turn and )?can\x27t be blocked this turn( except by Walls)?\.')
ACTIVATION = re.compile(r'(\{[1-9][0-9]*\}|\{R\})(, \{T\})?: (.+)')
MANA_MODE = '{T}: Add {C}.'
KEY_COST = "{T}, Discard a card: Up to one target creature can't be blocked this turn."
KEY_DRAW = 'Whenever this artifact becomes untapped, you may pay {2}. If you do, draw a card.'
ZERO = (0,)*6
COMMON = ('effect','baseline','expiry','replay','enemy_target','offturn','target_leaves_pending','source_leaves_pending')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or record.get('keywords')):
        raise ValueError('normal whole Oracle evasion source required')
    lines = record['oracle_text'].splitlines()
    source_type = record['type_line']
    if source_type=='Artifact' and lines==[KEY_COST,KEY_DRAW]:
        return dict(effect='optional_discard', cast_mana=cost_from_oracle(record), activation_mana=list(ZERO),
                    power_limit=None, haste=False, wall_exception=False, taps=True, untap_draw_fee=2)
    land = source_type=='Land' and len(lines)==2 and lines[0]==MANA_MODE
    single = len(lines)==1 and source_type in ('Artifact','Enchantment')
    cost = ACTIVATION.fullmatch(lines[-1]) if single or land else None
    target = TARGET.fullmatch(cost[3]) if cost else None
    if not target or bool(cost[2])!=(source_type!='Enchantment'):
        raise ValueError('complete targeted temporary unblockability wording required')
    return dict(effect='mana_sibling' if land else 'targeted', cast_mana=list(ZERO) if land else cost_from_oracle(record),
                activation_mana=cost_from_oracle(dict(record,mana_cost=cost[1])),
                power_limit=int(target[2]) if target[2] else None, haste=bool(target[3]),
                wall_exception=bool(target[4]), taps=bool(cost[2]), untap_draw_fee=0)


def variants(params):
    modes = list(COMMON)
    if params['effect']=='mana_sibling':
        modes += ['mana','exact_fee','initial_tapped','land_play']
    else:
        modes += ['cast','cast_short']
    if params['effect']=='optional_discard':
        modes += ['no_discard','untap_pay','untap_decline','untap_short','natural_untap','already_untapped']
    else:
        modes += ['no_creature']
        if params['effect']=='targeted':
            modes += ['fee_short','cancel']
    if params['taps'] and params['effect']!='mana_sibling':
        modes += ['initial_tapped']
    if params['power_limit'] is not None:
        modes += ['power_boundary','high_only','power_increased_pending']
    if params['wall_exception']:
        modes += ['wall_allowed']
    if params['haste']:
        modes += ['fresh_haste','fresh_without','haste_expiry','wrong_color']
    return [f'{mode}_{actor}' for mode in modes for actor in (0,1)] + (
        ['zero_targets_0'] if params['effect']=='optional_discard' else [])


def spec(declaration, record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'], {})
    selected = declaration.get('variants')
    if (not exact(pin.get('oracle'),record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned complete evasion source with exact typed variants required')
    return dict(model,family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources','witnesses'):
        for name,pin in pins()[group].items():
            row, record = rows.get(name,{}), oracle.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or row.get('kind')!='card'
                    or row.get('handler') in (None,'NULL','0','card_dummy') or record.get('layout')!='normal'):
                raise ValueError(f'implemented normal evasion source or witness changed: {name}')


class EvasionScene(Scene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        if mode in ('offturn','enemy_target'):
            self.active = self.foe
        self.recipient = None
        self.target = 'offturn_target' if mode=='offturn' else 'ally'
        self.fresh = mode in ('fresh_haste','fresh_without','haste_expiry')

    def setup_cards(self):
        mode = self.mode
        source_zone = 'hand' if mode in ('cast','cast_short','land_play') else 'battlefield'
        self.add(self.name,'source',self.owner,source_zone,tapped=mode in ('initial_tapped','natural_untap','untap_pay','untap_decline','untap_short'))
        if mode!='no_creature':
            witness = {'high_only':'Serra Angel','power_boundary':'Catapult Squad'}.get(mode,'Goblin Tinkerer')
            self.add(witness,'ally', self.active)
            if self.fresh:
                self.setup[-1]['state'] = ['sick']
            if mode!='high_only':
                self.add('Wall of Roots' if mode=='wall_allowed' else 'Goblin Tinkerer','blocker',1-self.active)
        if mode=='offturn':
            self.add('Goblin Tinkerer','offturn_target',self.owner)
        if self.effect=='optional_discard' and mode not in ('no_discard','cast','cast_short','untap_pay','untap_decline','untap_short','already_untapped','natural_untap'):
            self.add('Plains','discard',self.owner,'hand')
        if self.effect=='mana_sibling' and mode=='expiry':
            self.add('Sol Ring','mana_sink',self.owner,'hand')
        if mode in ('untap_pay','untap_decline','untap_short','already_untapped'):
            self.add('Twiddle','reply',self.foe,'hand')
        for response,name in [('target_leaves_pending','Unsummon'),('source_leaves_pending','Boomerang'),
                              ('power_increased_pending','Giant Growth')]:
            if mode==response:
                self.add(name,'reply',self.foe,'hand')
        if mode in ('untap_pay','untap_decline','untap_short','natural_untap','already_untapped'):
            self.add('Forest','drawn',self.owner,'library')
            self.add('Plains','next_card',self.owner,'library')

    def source_traits(self,full=False):
        self.prop('source','csvid',self.csvid)
        if full:
            self.emit('assert_matching_permanents',player=self.owner,index=self.csvid,property='artifact',
                      value=int(self.oracle[self.name]['type_line']=='Artifact'))

    def turn_setup(self):
        self.emit('turn',player=self.active,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')

    def cast_or_play(self):
        if self.mode=='land_play':
            self.emit('play_land',ref='source',value=1)
            self.entities['source']['zone'] = 'battlefield'
            self.mana_ability()
            return
        fee = self.model['cast_mana']
        budget = fee.copy()
        allowed = self.mode=='cast'
        if not allowed:
            budget[next(i for i,n in enumerate(budget) if n)] -= 1
        self.fund(self.owner,budget)
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(allowed))
        if allowed:
            self.pay(self.owner,fee)
            self.prop('source','in_hand',0)
            self.resolve()
            self.entities['source']['zone'] = 'battlefield'
        self.receipts()

    def mana_ability(self):
        self.emit('can_activate',ref='source',value=1)
        self.emit('activate',ref='source',value=1)
        self.entities['source']['tapped'] = True
        self.mana[self.owner][0] += 1
        self.receipts()
        self.emit('can_activate',ref='source',value=0)

    def activation(self):
        mode = self.mode
        fee = self.model['activation_mana']
        budget = fee.copy()
        if self.effect=='mana_sibling' and mode not in ('exact_fee','no_creature','initial_tapped'):
            budget[0] += 1
        if mode=='fee_short':
            budget[next(i for i,n in enumerate(fee) if n)] -= 1
        if mode=='wrong_color':
            budget[4],budget[3] = 0,1
        self.fund(self.owner,budget)
        if mode=='replay':
            self.replay = len(self.steps)
        available = mode not in ('fee_short','wrong_color','high_only','initial_tapped','no_discard','no_creature')
        self.emit('can_activate',ref='source',value=int(available or self.effect=='mana_sibling' and mode=='no_creature'))
        if not available and self.effect=='mana_sibling' and mode=='no_creature':
            self.mana_ability()
            return False
        if available:
            if self.effect=='mana_sibling':
                self.emit('choose_activation_mode',ref='source',player=self.owner,
                          mode_label="Give 'Unblockable' to a creature",fallback_color=0,allow_auto_nonmana=True)
            if self.effect=='optional_discard' and self.owner==0:
                self.emit('choose_zone_card',player=self.owner,value=self.rows['Plains']['csvid'])
            if mode in ('cancel','zero_targets'):
                self.emit('choose_cancel_target',player=self.owner)
            else:
                self.emit('choose_target',ref=self.target,player=self.owner)
        accepted = available and mode!='cancel'
        self.emit('activate',ref='source',value=int(accepted))
        if available and self.effect=='mana_sibling':
            self.emit('require_activation_mode',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(accepted))
        if not accepted:
            # A canceled target may be prompted after funding; the cancellation refunds any provisional payment.
            self.receipts()
            return False
        self.pay(self.owner,fee)
        self.entities['source']['tapped'] = self.model['taps']
        self.prop('source','tapped',self.model['taps'])
        if self.effect=='optional_discard':
            self.leave('discard','graveyard')
            self.emit('assert_zone_multiplicity',player=self.owner,zone='graveyard',index=self.rows['Plains']['csvid'],value=1)
            self.emit('assert_zone',player=self.owner,zone='hand',value=0)
        self.pools()
        return True

    def response(self,target):
        from recipes import cost_from_oracle

        name = self.entities['reply']['name']
        price = cost_from_oracle(self.oracle[name])
        self.fund(self.foe,price)
        self.emit('choose_target',player=self.foe,ref=target)
        self.emit('cast',ref='reply',value=1)
        self.emit('assert_stack',value=2)
        self.emit('assert_choices_consumed')
        self.pay(self.foe,price)
        self.entities['reply']['zone'] = 'stack'

    def after_activation(self):
        mode = self.mode
        if mode=='source_leaves_pending':
            self.response('source')
            self.leave('source','hand')
        elif mode in ('target_leaves_pending','power_increased_pending'):
            self.response('ally')
            if mode=='target_leaves_pending':
                self.leave('ally','hand')
            else:
                self.entities['ally']['bonus'] = (3,3)
        self.resolve()
        if 'reply' in self.entities:
            self.leave('reply','graveyard')
        if mode not in ('target_leaves_pending','power_increased_pending','zero_targets'):
            self.recipient = self.target
        if self.fresh:
            self.prop('ally','sick',int(not self.model['haste']))
        self.receipts()

    def spend_passage_residual(self):
        from recipes import cost_from_oracle

        fee = cost_from_oracle(self.oracle['Sol Ring'])
        self.emit('cast',ref='mana_sink',value=1)
        self.emit('assert_stack',value=1)
        self.pay(self.owner,fee)
        self.resolve()
        self.entities['mana_sink']['zone'] = 'battlefield'
        self.receipts()

    def combat(self):
        allowed = not self.fresh or self.model['haste'] and self.recipient=='ally'
        self.emit('begin_attackers')
        self.emit('attack',ref='ally',value=int(allowed))
        self.emit('finish_attackers')
        if not allowed:
            self.prop('ally','tapped',0)
            self.receipts()
            return
        self.entities['ally']['tapped'] = True
        self.emit('begin_blockers')
        blocked = self.recipient!='ally' or self.mode=='wall_allowed'
        self.emit('block',ref='blocker',other='ally',value=int(blocked))
        self.emit('advance_combat')
        self.prop('ally','blocked',blocked)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power,tough = self.stats('ally')
        if not blocked:
            self.life[1-self.active] -= power
        else:
            block_power,block_tough = self.stats('blocker')
            for target, damage, defense in [('ally',block_power,tough),('blocker',power,block_tough)]:
                if damage>=defense:
                    self.leave(target,'graveyard')
                else:
                    self.entities[target]['damage'] = damage
        self.receipts()

    def untap_key(self):
        if self.mode=='natural_untap':
            self.advance()
            self.advance()
            self.emit('untap_step',player=self.owner)
            self.entities['source']['tapped'] = False
            self.receipts()
            return
        fee = self.model['untap_draw_fee']
        budget = fee-int(self.mode=='untap_short')
        self.fund(self.owner,(budget,0,0,0,0,0))
        self.cast_spell('reply','source')
        self.emit('choose_dialog',player=self.foe,value=1)
        if self.mode in ('untap_pay','untap_decline'):
            self.emit('choose_dialog',player=self.owner,value=0 if self.mode=='untap_pay' else 1)
        if self.mode=='untap_pay':
            self.pay(self.owner,(fee,0,0,0,0,0))
            self.leave('drawn','hand')
        self.resolve()
        self.leave('reply','graveyard')
        self.entities['source']['tapped'] = False
        self.receipts()
        top = 'Plains' if self.mode=='untap_pay' else 'Forest'
        self.emit('assert_zone_card',player=self.owner,zone='library',index=0,value=self.rows[top]['csvid'])

    def play(self):
        self.setup_cards()
        self.turn_setup()
        mode = self.mode
        if mode in ('cast','cast_short','land_play'):
            self.cast_or_play()
        elif mode=='mana':
            self.mana_ability()
        elif mode in ('untap_pay','untap_decline','untap_short','already_untapped','natural_untap'):
            self.untap_key()
        elif mode in ('baseline','fresh_without'):
            self.combat()
        elif self.activation():
            self.after_activation()
            if mode=='haste_expiry':
                self.cleanup()
                self.recipient = None
                self.prop('ally','sick',1)
                return
            if self.effect=='mana_sibling' and mode=='expiry':
                self.spend_passage_residual()
            if mode!='target_leaves_pending':
                self.combat()
            if mode=='expiry':
                self.advance()
                self.advance()
                self.emit('untap_step',player=self.active)
                for e in self.entities.values():
                    if e['controller']==self.active and e['zone']=='battlefield':
                        e['tapped'] = False
                self.recipient = None
                self.emit('turn',player=self.active,value=0)
                self.combat()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved temporary evasion source')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene = EvasionScene(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [step('record_state'),step('ai_roundtrip'),step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Multiplayer']
        if model['untap_draw_fee']:
            gaps += ['Untap-trigger response window under the legacy synchronous EVENT_UNTAP_CARD callback',
                     'AI voluntary zero-target choice']
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                        covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
        cases.append(dict(name=f'ActivatedUnblockabilityRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=1732,setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
