"""Actual Aura-granted tap damage attributed to its enchanted creature."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_turn_up_target_recipes import Scene


FAMILY = 'aura_granted_damage'
PROFILE = Path(__file__).parent / 'data/aura_granted_damage_wave8_profiles.json'
CLAUSES = {
    'Enchanted creature has "{T}: This creature deals 1 damage to any target."': 'fixed',
    'Enchanted creature has "{T}: This creature deals damage equal to its power to any target."': 'power',
}
MODES = ('player', 'self_player', 'creature', 'nonlethal', 'walker', 'short', 'wrong_color', 'no_creature',
         'fresh', 'tapped', 'cancel', 'foreign_host', 'changed_control', 'repeat', 'host_lifelink', 'source_color',
         'target_leaves', 'aura_leaves', 'aura_removed_pending', 'host_removed_pending', 'cast_host_leaves',
         'replay', 'pending_replay')
POWER = ('power_up_pending', 'power_down_pending', 'power_zero', 'power_last_known')
SPELL_EFFECTS = {
    'Giant Growth': (3, 3),
    'Disfigure': (-2, -2),
}


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout') != 'normal' or record.get('selected_face') or 'card_faces' in record or
            record.get('type_line') != 'Enchantment — Aura' or record.get('keywords') != ['Enchant']):
        raise ValueError('complete normal Aura required')
    lines = record['oracle_text'].splitlines()
    if len(lines) != 2 or lines[0] != 'Enchant creature' or lines[1] not in CLAUSES:
        raise ValueError('complete creature Aura with reviewed tap damage required')
    return dict(effect=CLAUSES[lines[1]], cast_mana=cost_from_oracle(record))


def variants(model):
    modes = MODES + POWER if model['effect']=='power' else MODES
    return [f'{mode}_{actor}' for actor in (0,1) for mode in modes]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    pin = pins()['sources'].get(record['name'], {})
    if (not exact(pin.get('oracle'), record) or declaration.get('family') != FAMILY or
            type(declaration.get('version')) is not int or declaration['version'] != 2 or
            declaration.get('oracle_text') != record['oracle_text'] or not exact(declaration.get('params'), model) or
            type(selected) is not list or not selected or any(type(v) is not str for v in selected) or
            len(set(selected)) != len(selected) or set(selected) - set(variants(model))):
        raise ValueError('independently pinned damage Aura and typed distinct variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    for group in ('sources','witnesses'):
        for name,pin in pins()[group].items():
            row = rows.get(name, {})
            if (not exact(pin, dict(oracle=oracle.get(name), registry=row)) or row.get('kind') != 'card' or
                    row.get('handler') in (None, 'NULL', '0', 'card_dummy') or oracle[name].get('layout') != 'normal'):
                raise ValueError(f'implemented original Aura or witness changed: {name}')


class DamageAuraScene(Scene):
    def __init__(self, name, model, mode, owner, rows, oracle):
        super().__init__(name, model, mode, owner, rows, oracle)
        self.loyalty = None
        self.attachment = False

    def setup_cards(self):
        self.add(self.name, 'source', self.owner, 'hand')
        mode = self.mode
        if mode=='no_creature':
            self.add('Forest', 'noncreature', self.foe, tapped=True)
            return
        if mode in ('host_lifelink', 'foreign_host', 'source_color'):
            host_name = 'Child of Night'
        elif mode=='power_zero':
            host_name = 'Wall of Tanglecord'
        else:
            host_name = 'Serra Angel'
        host_owner = self.foe if mode=='foreign_host' else self.owner
        self.add(host_name, 'host', host_owner, 'hand' if mode=='fresh' else 'battlefield', tapped=mode=='tapped')
        if mode=='creature':
            self.add('Mogg Fanatic', 'victim', self.foe)
        if mode in ('nonlethal', 'target_leaves'):
            self.add('Shivan Dragon', 'victim', self.foe)
        if mode=='walker':
            self.add('Jace Beleren', 'victim', self.foe)
            self.loyalty = int(self.oracle['Jace Beleren']['loyalty'])
        if mode=='source_color':
            self.add('White Knight', 'protected', self.foe)
        if mode=='changed_control':
            self.add('Bazaar Trader', 'trader', self.owner)
        respond = {
            'target_leaves':'Unsummon', 'cast_host_leaves':'Unsummon',
            'aura_leaves':'Disenchant', 'aura_removed_pending':'Disenchant',
            'host_removed_pending':'Murder', 'power_last_known':'Murder',
            'power_up_pending':'Giant Growth', 'power_down_pending':'Disfigure',
        }.get(mode)
        if respond:
            actor = self.owner if respond=='Giant Growth' else self.foe
            self.add(respond, 'answer', actor, 'hand')
        if mode=='repeat':
            self.add('Twiddle', 'untap', self.owner, 'hand')
        if mode=='power_last_known':
            self.add('Giant Growth', 'boost', self.owner, 'hand')

    def receipts(self):
        super().receipts()
        if self.loyalty is not None and self.entities['victim']['zone']=='battlefield':
            self.emit('assert_counter', ref='victim', counter_kind='loyalty', value=self.loyalty)

    def leave(self, ref, zone):
        super().leave(ref, zone)
        if ref in ('source', 'host'):
            self.attachment = False

    def begin_spell(self, ref, target=None, depth=1, response=False):
        from recipes import cost_from_oracle

        spell = self.entities[ref]
        actor = spell['owner']
        fee = cost_from_oracle(self.oracle[spell['name']])
        self.fund(actor, fee)
        if target is not None:
            self.emit('choose_target', player=actor, ref=target)
        self.emit('cast_response' if response else 'cast', ref=ref, value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=depth)
        self.pay(actor, fee)
        spell['zone'] = 'stack'
        self.pools()

    def cast_aura(self):
        mode = self.mode
        fee = self.model['cast_mana']
        budget = list(fee)
        if mode=='short':
            budget[0] -= 1
        if mode=='wrong_color':
            color = next(i for i,n in enumerate(fee) if i and n)
            budget[color] -= 1
            budget[3] += 1
        self.fund(self.owner, budget)
        allowed = mode not in ('short', 'wrong_color', 'no_creature')
        if allowed:
            self.emit('choose_target', player=self.owner, ref='host')
        self.emit('cast', ref='source', value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(allowed))
        if not allowed:
            self.receipts()
            return False
        self.pay(self.owner, fee)
        self.entities['source']['zone'] = 'stack'
        self.pools()
        if mode=='cast_host_leaves':
            self.begin_spell('answer', target='host', depth=2, response=True)
            self.resolve(parent=True)
            self.leave('answer', 'graveyard')
            self.leave('host', 'hand')
            self.leave('source', 'graveyard')
            self.receipts()
            return False
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.attachment = True
        self.receipts()
        return True

    def host_actor(self):
        return self.entities['host']['controller']

    def chooser(self):
        # Production Hermetic Study supplies the live host as target context.
        # Burning Anger's legacy selector uses the Aura context. This maps UI
        # input only. All cost, recipient, and damage-source expectations use
        # the printed creature ability and its actual controller.
        return self.owner if self.effect=='power' else self.host_actor()

    def expected_damage(self):
        return 1 if self.effect=='fixed' else max(0, self.stats('host')[0])

    def begin_ping(self, recipient, cancel=False, available=True):
        if not self.attachment:
            raise ValueError('a granted activation requires the named live attachment')
        self.emit('can_activate', ref='source', value=int(available))
        if available:
            chooser = self.chooser()
            if cancel:
                self.emit('choose_cancel_target', player=chooser)
            elif isinstance(recipient, int):
                self.emit('choose_player', player=chooser, value=recipient)
            else:
                self.emit('choose_target', player=chooser, ref=recipient)
        accepted = available and not cancel
        self.emit('activate', ref='source', value=int(accepted))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack', value=int(accepted))
        if accepted:
            self.entities['host']['tapped'] = True
            self.prop('host','tapped',1)
            self.prop('source','tapped',0)
            for player,n in enumerate(self.life):
                self.emit('assert_life', player=player, value=n)
            if recipient=='victim':
                if self.loyalty is None:
                    self.prop('victim','damage',0)
                else:
                    self.emit('assert_counter', ref='victim', counter_kind='loyalty', value=self.loyalty)
        else:
            self.receipts()

    def resolve_hit(self, recipient, amount, actor, lifelink):
        self.resolve()
        if isinstance(recipient, int):
            self.life[recipient] -= amount
        elif self.entities[recipient]['zone']!='battlefield':
            amount = 0
        elif self.loyalty is not None:
            self.loyalty = max(0, self.loyalty-amount)
            if self.loyalty==0:
                self.leave(recipient,'graveyard')
        else:
            target = self.entities[recipient]
            target['damage'] += amount
            if target['damage']>=self.stats(recipient)[1]:
                self.leave(recipient,'graveyard')
        if lifelink:
            self.life[actor] += amount
        self.receipts()

    def modify_host(self, ref):
        stat = SPELL_EFFECTS[self.entities[ref]['name']]
        self.entities['host']['bonus'] = tuple(a+b for a,b in zip(self.entities['host']['bonus'],stat))
        self.leave(ref,'graveyard')

    def apply_response(self):
        mode = self.mode
        if mode in ('power_up_pending','power_down_pending'):
            self.begin_spell('answer','host',depth=2)
            self.modify_host('answer')
        elif mode in ('host_removed_pending','power_last_known'):
            self.begin_spell('answer','host',depth=2)
            self.leave('answer','graveyard')
            self.leave('host','graveyard')
            self.leave('source','graveyard')
        elif mode=='aura_removed_pending':
            self.begin_spell('answer','source',depth=2)
            self.leave('answer','graveyard')
            self.leave('source','graveyard')
        elif mode=='target_leaves':
            self.begin_spell('answer','victim',depth=2)
            self.leave('answer','graveyard')
            self.leave('victim','hand')

    def play(self):
        self.setup_cards()
        self.emit('turn', player=self.owner, value=0)
        self.emit('present_ai_dialogs', value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana', player=player, color=color, value=0)
        self.emit('refresh')
        mode = self.mode
        if mode=='fresh':
            self.begin_spell('host')
            self.resolve()
            self.entities['host']['zone'] = 'battlefield'
        if not self.cast_aura():
            return
        if mode=='changed_control':
            self.donate('host')
            # The donated host must survive an actual turn change before it
            # can pay a tap cost under its new controller.
            self.advance()
            self.emit('refresh')
        if mode=='aura_leaves':
            self.begin_spell('answer','source')
            self.resolve()
            self.leave('answer','graveyard')
            self.leave('source','graveyard')
            # The provider reference is no longer a real permanent. Verify
            # empty stack and a surviving untapped host via exact receipts.
            self.receipts()
            return
        if mode in ('fresh','tapped','cancel'):
            self.begin_ping(self.foe, cancel=mode=='cancel', available=mode=='cancel')
            return
        if mode=='source_color':
            self.emit('probe_activation_target',ref='source',other='protected',index=-1,value=0)
        if mode=='power_last_known':
            self.begin_spell('boost','host')
            self.resolve()
            self.modify_host('boost')
            self.receipts()
        actor = self.host_actor()
        target = 'victim' if mode in ('creature','nonlethal','walker','target_leaves') else actor if mode=='self_player' else 1-actor
        lifelink = 'Lifelink' in self.oracle[self.entities['host']['name']]['keywords']
        if mode=='replay':
            self.replay = len(self.steps)
        self.begin_ping(target)
        if mode=='pending_replay':
            self.replay = len(self.steps)
        last_power = self.expected_damage()
        self.apply_response()
        amount = self.expected_damage() if self.entities['host']['zone']=='battlefield' else last_power
        self.resolve_hit(target, amount, actor, lifelink)
        if mode=='repeat':
            self.begin_ping(target, available=False)
            self.begin_spell('untap','host')
            self.emit('choose_dialog',player=self.owner,value=1)
            self.resolve()
            self.leave('untap','graveyard')
            self.entities['host']['tapped'] = False
            self.receipts()
            self.begin_ping(target)
            self.resolve_hit(target, self.expected_damage(), actor, lifelink)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if csvid != pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved damage Aura source')
    cases = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene = DamageAuraScene(name, model, mode, int(actor), rows, oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at] + [step('capture')] + suffix + [
                step('record_state'), step('ai_roundtrip'), step('restore')] + suffix + [step('assert_recorded_state')]
        gaps = ['Gameplay UI discovery and separate controller authorization for an opponent-owned Aura grant', 'Multiplayer']
        metadata = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                        oracle_text=declaration['oracle_text'],covered_clauses=declaration['oracle_text'].splitlines(),
                        untested_components=gaps)
        cases.append(dict(name=f'AuraGrantedDamageRecipe_{csvid}_{variant}',card=name,fixture='empty_main',
                          seed=123,setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
