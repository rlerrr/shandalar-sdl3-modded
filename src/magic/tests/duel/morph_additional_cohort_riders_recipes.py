"""Paid Morph and additional mandatory turn-up target effects."""

from functools import lru_cache
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from morph_creature_recipes import FACE_DOWN, MORPH
from morph_turn_up_target_recipes import Scene


FAMILY = 'morph_additional_cohort_riders'
PROFILE = Path(__file__).parent/'data/morph_additional_cohort_riders_wave8_profiles.json'
TRIGGER = 'When this creature is turned face up, '
RULES = {
    (TRIGGER+'destroy target artifact or enchantment.',): ('disenchant', ()),
    (TRIGGER+'target creature gets +3/+3 until end of turn.',): ('grow', ()),
    (TRIGGER+"destroy target nonblack creature. It can't be regenerated.",): ('bury', ()),
    (TRIGGER+'it deals 2 damage to any target.',): ('damage', ()),
    ('Flying', TRIGGER+'target creature you control gains hexproof until end of turn.'): ('hexproof', ('Flying',)),
}
COMMON = ('printed', 'hidden', 'flip', 'flip_short', 'replay')
EXTRA = {
    'disenchant': ('enchantment', 'type_probe', 'indestructible', 'no_target'),
    'grow': ('own_target', 'self_target', 'offturn', 'expiry'),
    'bury': ('type_probe', 'cannot_regenerate', 'indestructible', 'no_target'),
    'damage': ('player_target', 'walker_target', 'survive_damage', 'own_player'),
    'hexproof': ('type_probe', 'expiry', 'flying_combat', 'hidden_combat'),
}
WITNESSES = {'Jalum Tome', 'Fervor', 'Darksteel Relic', 'Darksteel Myr', 'Grizzly Bears',
             'Hill Giant', 'Drudge Skeletons', 'Uthden Troll', 'Jace Beleren', 'Unsummon', 'Doom Blade'}
VANILLA = {'Grizzly Bears', 'Hill Giant'}


@lru_cache(maxsize=1)
def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    matches = [(index, match) for index, line in enumerate(lines) if (match := MORPH.fullmatch(line))]
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record['type_line'].startswith('Creature — ') or len(matches) != 1):
        raise ValueError('whole normal creature with one paid Morph clause required')
    index, morph = matches[0]
    role = RULES.get(tuple(lines[:index] + lines[index+1:]))
    stats = (record['power'], record['toughness'])
    if not role or any(type(stat) is not str or not stat.isascii() or not stat.isdecimal() for stat in stats):
        raise ValueError('complete mandatory Morph rider and fixed stats required')
    effect, keywords = role
    if sorted(record['keywords']) != sorted(('Morph', *keywords)):
        raise ValueError('printed Morph keywords do not match the exact rider')
    cast = cost_from_oracle(record)
    flip = cost_from_oracle(dict(record, mana_cost=morph[1]))
    if not any(flip[1:]) or sum(value != 0 for value in flip[1:]) != 1:
        raise ValueError('fixed colored Morph payment required')
    return dict(effect=effect, keywords=list(keywords), cast_mana=cast, flip_mana=flip,
                power=int(stats[0]), toughness=int(stats[1]))


def variants(params):
    return [f'{mode}_{owner}' for owner in (0,1) for mode in COMMON + EXTRA[params['effect']]]


def spec(declaration, record):
    model = parameters(record)
    selected = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or declaration.get('oracle_text') != record['oracle_text']
            or not exact(profile()['sources'].get(record['name'],{}).get('oracle'), record)
            or not exact(declaration.get('params'), model)):
        raise ValueError('exact pinned whole Morph source and rider required')
    if (not isinstance(selected,list) or not selected or any(type(item) is not str for item in selected)
            or len(selected) != len(set(selected)) or set(selected)-set(variants(model))):
        raise ValueError('distinct reviewed Morph variants required')
    return dict(model, family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profile()
    if set(pins['witnesses']) != WITNESSES or len(pins['sources']) != len(RULES):
        raise ValueError('complete Morph cohort and witness scope required')
    if pins['face_down']['csvid'] != FACE_DOWN or not exact(pins['face_down'], rows['Face-Down Creature']):
        raise ValueError('native face-down observation identity changed')
    for name, pin in (pins['sources'] | pins['witnesses']).items():
        row = rows[name]
        if (not exact(oracle[name], pin['oracle']) or not exact(row, pin['registry']) or row['kind'] != 'card'
                or row['handler'] in (None,'NULL','0') or row['handler'] == 'card_dummy' and name not in VANILLA):
            raise ValueError(f'pinned implementation identity changed: {name}')


class TargetScene(Scene):
    def setup_cards(self):
        self.ward = None
        self.walker = None
        self.add(self.name, 'source', self.owner, 'hand')
        effect, mode = self.effect, self.mode
        if mode == 'no_target':
            if effect == 'bury':
                self.add('Drudge Skeletons', 'wrong_target', self.foe)
            return
        if effect == 'disenchant':
            name = {'enchantment':'Fervor','indestructible':'Darksteel Relic'}.get(mode, 'Jalum Tome')
        elif effect == 'bury':
            name = {'cannot_regenerate':'Uthden Troll','indestructible':'Darksteel Myr'}.get(mode, 'Grizzly Bears')
        elif effect == 'damage' and mode == 'walker_target':
            name = 'Jace Beleren'
            self.walker = int(self.oracle[name]['loyalty'])
        else:
            name = 'Hill Giant' if mode == 'survive_damage' else 'Grizzly Bears'
        actor = self.owner if effect == 'hexproof' or mode == 'own_target' else self.foe
        self.add(name, 'victim', actor)
        if mode == 'type_probe':
            name = {'disenchant':'Grizzly Bears','bury':'Drudge Skeletons','hexproof':'Grizzly Bears'}[effect]
            self.add(name, 'wrong_target', self.foe)
        if mode in ('flying_combat','hidden_combat'):
            self.add('Grizzly Bears', 'blocker', self.foe)
        if effect == 'hexproof':
            self.add('Unsummon', 'foreign_spell', self.foe, 'hand')
            self.add('Unsummon', 'own_spell', self.owner, 'hand')
        if mode == 'cannot_regenerate':
            self.add('Doom Blade', 'destroy', self.owner, 'hand')

    def receipts(self):
        super().receipts()
        if self.walker is not None:
            self.emit('assert_counter',ref='victim',counter_kind='loyalty',value=self.walker)

    def ward_receipt(self, target, protected):
        self.emit('can_target_creature',ref='foreign_spell',other=target,value=int(not protected))
        self.emit('can_target_creature',ref='own_spell',other=target,value=1)

    def regeneration_control(self):
        self.cast_spell('destroy','victim')
        self.fund(self.foe, [0,0,0,0,1,0])
        self.emit('choose_regeneration',ref='victim')
        self.resolve()
        self.pay(self.foe, [0,0,0,0,1,0])
        self.leave('destroy','graveyard')
        self.entities['victim']['tapped'] = True
        self.receipts()

    def selected_target(self):
        if self.mode == 'no_target':
            return None
        return 'source' if self.mode == 'self_target' else 'victim'

    def flip(self):
        fee = self.model['flip_mana']
        budget = list(fee)
        short = self.mode == 'flip_short'
        if short:
            budget[next(i for i,v in enumerate(fee) if i and v)] -= 1
        self.fund(self.owner, budget)
        if self.mode == 'replay':
            self.replay = len(self.steps)
        self.emit('can_activate',ref='source',value=int(not short))
        if self.mode == 'type_probe':
            self.emit('probe_activation_target',ref='source',other='wrong_target',index=-1,value=0)
            self.emit('probe_activation_target',ref='source',other='victim',index=-1,value=1)
        target = self.selected_target()
        if short:
            self.emit('activate',ref='source',value=0)
            self.receipts()
            return
        if self.mode in ('player_target','own_player'):
            player = self.foe if self.mode == 'player_target' else self.owner
            self.emit('choose_player',player=self.owner,value=player)
        elif target is not None:
            self.emit('choose_target',player=self.owner,ref=target)
        unexpected_self_offer = self.mode == 'no_target' and self.effect == 'bury'
        if unexpected_self_offer:
            # A legal turn-up offers no such target. Observe whether the
            # actual legacy picker accepts the still-colorless source.
            self.emit('choose_target',player=self.owner,ref='source')
        if self.mode == 'cannot_regenerate':
            self.fund(self.foe, [0,0,0,0,1,0])
            self.emit('choose_regeneration',ref='victim')
        self.emit('activate',ref='source',value=1)
        self.pay(self.owner, fee)
        self.hidden = False
        self.emit('assert_unused_choices',value=int(self.mode == 'cannot_regenerate' or unexpected_self_offer))
        self.emit('refresh')
        self.pools()
        self.resolve()
        self.apply_rider(target)
        self.receipts()

    def apply_rider(self, target):
        if target is None:
            return
        if self.effect in ('disenchant','bury'):
            if self.mode != 'indestructible':
                self.leave(target,'graveyard')
        elif self.effect == 'grow':
            self.entities[target]['bonus'] = (3,3)
        elif self.effect == 'damage':
            if self.mode in ('player_target','own_player'):
                self.life[self.foe if self.mode == 'player_target' else self.owner] -= 2
            elif self.walker is not None:
                self.walker -= 2
            elif self.mode == 'survive_damage':
                self.entities[target]['damage'] = 2
            else:
                self.leave(target,'graveyard')
        else:
            self.ward = target
            self.ward_receipt(target, True)

    def combat(self):
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.entities['source']['tapped'] = True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='source',value=int(self.hidden))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if self.hidden:
            self.leave('source','graveyard')
            self.leave('blocker','graveyard')
        else:
            self.life[self.foe] -= self.model['power']
        self.receipts()

    def cast_both_available(self, *, faceup):
        fee = self.model['cast_mana']
        self.fund(self.owner, [max(3,fee[0]),*fee[1:]])
        self.emit('choose_dialog',player=self.owner,value=int(faceup))
        self.emit('cast',ref='source',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.prop('source','in_hand',0)
        self.pay(self.owner, fee if faceup else [3,0,0,0,0,0])
        self.hidden = not faceup
        self.source_traits()
        self.pools()
        self.resolve()
        self.entities['source']['zone'] = 'battlefield'
        self.receipts()
        for color, value in enumerate(self.mana[self.owner]):
            if value:
                self.emit('set_mana',player=self.owner,color=color,value=0)
        self.mana[self.owner] = [0]*6
        self.emit('refresh')

    def play(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')
        self.cast_both_available(faceup=self.mode == 'printed')
        if self.effect == 'hexproof':
            self.ward_receipt('victim', False)
        if self.mode in ('printed','hidden'):
            return
        if self.mode == 'offturn':
            self.advance()
        if self.mode in ('flying_combat','hidden_combat'):
            self.advance()
            self.advance()
        if self.mode == 'cannot_regenerate':
            self.regeneration_control()
        if self.mode != 'hidden_combat':
            self.flip()
        if self.mode in ('flying_combat','hidden_combat'):
            self.combat()
        if self.mode == 'expiry':
            self.cleanup()
            if self.ward:
                self.ward_receipt(self.ward, False)


def expand(name, csvid, declaration, model, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle, rows)
    if rows[name]['csvid'] != csvid or not selected or set(selected)-set(variants(model)):
        raise ValueError('unbound Morph source or variant')
    metadata = dict(family=FAMILY,version=2,params=declaration['params'],oracle_text=declaration['oracle_text'],
                    covered_clauses=declaration['oracle_text'].splitlines(),
                    untested_components=['Separate priority after selecting a target for the real turn-up trigger',
                                         'Multiplayer and replacement effects'])
    cases = []
    for variant in selected:
        mode, owner = variant.rsplit('_',1)
        scene = TargetScene(name,model,mode,int(owner),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            tail = steps[at:]
            steps = [*steps[:at],step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                     step('restore'),*tail,step('assert_recorded_state')]
        cases.append(dict(name=f'MorphAdditionalCohortRecipe_{csvid}_{variant}', card=name, fixture='empty_main',
                          seed=2047, setup=scene.setup, steps=steps, _recipe=dict(metadata,variant=variant)))
    return cases
