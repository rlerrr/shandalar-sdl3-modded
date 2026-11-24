"""Typed turn-up counters against a real announced parent spell."""

from copy import deepcopy
from functools import lru_cache
import hashlib
import json
from pathlib import Path

from death_counter_trigger_recipes import exact
from megamorph_keyword_creatures_recipes import MEGAMORPH
from morph_creature_recipes import FACE_DOWN, MORPH
from morph_turn_up_target_recipes import Scene
from recipes import cost_from_oracle, step


FAMILY = 'pending_spell_morph'
PROFILE = Path(__file__).with_name('data') / 'pending_spell_morph_wave8_profiles.json'
TRIGGER = 'When this creature is turned face up, counter target '
CLAUSES = {
    'plain': TRIGGER + 'spell.',
    'typed': TRIGGER + 'instant or sorcery spell.',
    'tax': TRIGGER + 'spell unless its controller pays {3}.',
    'exile': (TRIGGER + "spell. If that spell is countered this way, exile it instead of putting it into its owner's graveyard. "
              'You may cast that card without paying its mana cost for as long as it remains exiled.'),
}
COMMON = ('ordinary', 'short_hidden', 'no_spell', 'instant', 'sorcery', 'permanent', 'own_spell',
          'short_flip', 'wrong_color', 'snapshot', 'bounce_reset')
EXTRA = {
    'plain': ('uncounterable',),
    'typed': ('flying', 'hidden_ground'),
    'tax': ('tax_paid', 'tax_declined', 'tax_short', 'uncounterable'),
    'exile': ('uncounterable', 'play_foreign', 'play_own', 'permission_persists',
              'creature_offturn', 'wrong_actor', 'play_replay'),
}
NO_PARENT = ('ordinary', 'short_hidden', 'no_spell', 'bounce_reset', 'flying', 'hidden_ground')
FREE_INSTANT_MODES = ('play_foreign', 'play_own', 'permission_persists', 'play_replay')
FREE_PLAY = FREE_INSTANT_MODES + ('creature_offturn',)


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    text = record.get('oracle_text', '').splitlines()
    if (record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face')
            or not record.get('type_line', '').startswith('Creature — ') or len(text) < 2):
        raise ValueError('complete original Morph creature required')
    effect = next((kind for kind, clause in CLAUSES.items() if text[-1] == clause), None)
    mega = effect in ('typed','tax')
    flying = effect == 'typed'
    match = (MEGAMORPH if mega else MORPH).fullmatch(text[-2])
    expected_keywords = (['Flying'] if flying else []) + ['Megamorph' if mega else 'Morph']
    if (effect is None or not match or text[:-2] != (['Flying'] if flying else [])
            or record.get('keywords') != expected_keywords or record.get('colors') != ['U']):
        raise ValueError('reviewed full Morph cost, keyword and pending-spell text required')
    return dict(effect=effect, megamorph=mega, flying=flying, cast_mana=cost_from_oracle(record),
                flip_mana=cost_from_oracle(dict(record,mana_cost=match[1])),
                power=int(record['power']), toughness=int(record['toughness']))


def variants(model):
    return [f'{mode}_{owner}' for mode in COMMON+EXTRA[model['effect']] for owner in (0,1)]


def spec(declaration, record):
    params = parameters(record)
    choice = declaration.get('variants')
    pin = profiles()['sources'].get(record['name'], {})
    if (not exact(pin.get('oracle'),record) or declaration.get('family')!=FAMILY or
            type(declaration.get('version')) is not int or declaration['version']!=2 or
            declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)
            or type(choice) is not list or not choice or any(type(i) is not str for i in choice)
            or len(set(choice))!=len(choice) or set(choice)-set(variants(params))):
        raise ValueError('exact pinned Morph declarations and distinct reviewed variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle, rows):
    data = profiles()
    unused = json.loads((PROFILE.parent/'unimplemented.json').read_text(encoding='utf8'))['cases']
    unimplemented = {row['csvid'] for row in unused.values()}
    root = Path(__file__).resolve().parents[4]
    for name,pin in (data['sources']|data['witnesses']).items():
        row = rows.get(name,{})
        if (not exact(pin['oracle'],oracle.get(name)) or not exact(pin['registry'],row) or row.get('kind')!='card'
                or row['csvid'] in unimplemented or row['handler'] in (None,'NULL','0','card_dummy')
                or oracle[name]['layout']!='normal'):
            raise ValueError('implemented pending Morph card changed: '+name)
        compiled = pin.get('compiled_source')
        if compiled and hashlib.sha256((root/compiled['path']).read_bytes()).hexdigest()!=compiled['sha256']:
            raise ValueError('compiled pending Morph source changed: '+name)
    if data['face_down']!=rows.get('Face-Down Creature') or data['face_down']['csvid']!=FACE_DOWN:
        raise ValueError('live native face-down observer changed')


class PendingMorphScene(Scene):
    def __init__(self,name,model,mode,owner,rows,oracle):
        super().__init__(name,model,mode,owner,rows,oracle)
        self.counters = 0
        self.actor = self.owner if mode in ('own_spell','play_own') else self.foe
        self.witness = {'sorcery':'Divination','permanent':'Crater Elemental',
                        'creature_offturn':'Crater Elemental','uncounterable':'Thrun, the Last Troll'}.get(
                            mode, 'Visions of Beyond' if mode in FREE_INSTANT_MODES else 'Lightning Bolt')

    def setup_cards(self):
        self.add(self.name,'source',self.owner,'hand')
        if self.mode not in NO_PARENT:
            self.add(self.witness,'parent',self.actor,'hand')
        if self.mode=='sorcery':
            for i,name in enumerate(('Forest','Island','Mountain')):
                self.add(name,f'library_{i}',self.actor,'library')
        if self.mode in FREE_INSTANT_MODES:
            for seat,deck in enumerate((('Forest','Island','Mountain'),('Plains','Mountain','Island'))):
                for index,name in enumerate(deck):
                    self.add(name,f'draw_library_{seat}_{index}',seat,'library')
        if self.mode in ('bounce_reset','permission_persists'):
            self.add('Unsummon','bounce',self.owner,'hand')
        if self.mode in ('flying','hidden_ground'):
            self.add('Elvish Mystic','ground',self.foe)
        if self.mode=='flying':
            self.add('Nessian Asp','reach',self.foe)
        if self.mode=='wrong_actor':
            self.add('Island','other_exile',self.foe,'exile')

    def stats(self,ref):
        power,toughness=super().stats(ref)
        bonus=self.counters if ref=='source' else 0
        return power+bonus,toughness+bonus

    def receipts(self):
        super().receipts()
        if self.entities['source']['zone']=='battlefield':
            self.prop('source','plus1_counters',self.counters)
        if self.mode=='sorcery':
            library=[e for e in self.entities.values() if e['zone']=='library']
            for i,e in enumerate(library):
                self.emit('assert_zone_card',player=self.actor,zone='library',index=i,value=self.rows[e['name']]['csvid'])

        if self.mode in FREE_INSTANT_MODES:
            for seat in (0,1):
                library=[e for e in self.entities.values() if e['owner']==seat and e['zone']=='library']
                for i,e in enumerate(library):
                    self.emit('assert_zone_card',player=seat,zone='library',index=i,value=self.rows[e['name']]['csvid'])

    def start(self):
        self.setup_cards()
        self.emit('turn',player=self.owner,value=0)
        self.emit('present_ai_dialogs',value=1)
        for player in (0,1):
            for color in range(6):
                self.emit('set_mana',player=player,color=color,value=0)
        self.emit('refresh')

    def source_cast(self,hidden=True,allowed=True):
        printed = self.model['cast_mana']
        cost = [3,0,0,0,0,0] if hidden else printed
        # Fund both forms to obtain the actual Morph choice on either seat.
        # Observe the payment before retiring unused fixture funding.
        budget = [max(3,printed[0]),*printed[1:]] if allowed else [2,0,0,0,0,0]
        self.fund(self.owner,budget)
        if allowed:
            self.emit('choose_dialog',player=self.owner,value=int(not hidden))
        self.emit('cast',ref='source',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.emit('assert_stack',value=int(allowed))
        self.resolve()
        if allowed:
            self.pay(self.owner,cost)
            self.entities['source']['zone']='battlefield'
            self.hidden=hidden
            self.counters=0
        self.receipts()
        if allowed and hidden:
            for color,value in enumerate(self.mana[self.owner]):
                if value:
                    self.emit('set_mana',player=self.owner,color=color,value=0)
            self.mana[self.owner]=[0]*6
            self.emit('refresh')

    def parent_cast(self,tax):
        if self.witness in ('Divination','Crater Elemental','Thrun, the Last Troll'):
            self.advance()
        fee = cost_from_oracle(self.oracle[self.witness])
        self.fund(self.actor,[fee[0]+tax,*fee[1:]])
        if self.witness=='Lightning Bolt':
            self.emit('choose_player',player=self.actor,value=1-self.actor)
        self.emit('cast',ref='parent',value=1)
        self.pay(self.actor,fee)
        self.entities['parent']['zone']='stack'
        self.emit('announce_spell',ref='parent')
        self.emit('assert_stack',value=1)
        self.prop('parent','in_hand',0)
        self.emit('assert_unused_choices',value=0)
        for player,value in enumerate(self.life):
            self.emit('assert_life',player=player,value=value)
        self.pools()

    def flip(self,pending,tax_payment):
        mode=self.mode
        cost=self.model['flip_mana']
        budget=list(cost)
        if mode=='short_flip':
            budget[0]-=1
        if mode=='wrong_color':
            budget[2]-=1
            budget[3]+=1
        allowed=mode not in ('short_flip','wrong_color')
        self.fund(self.owner,budget)
        if mode=='snapshot':
            self.replay=len(self.steps)
        if tax_payment is not None:
            self.emit('choose_dialog',player=self.actor,value=int(not tax_payment))
        if not pending:
            self.emit('can_activate',ref='source',value=int(allowed))
        self.emit('activate_response' if pending else 'activate',ref='source',value=int(allowed))
        if allowed:
            self.pay(self.owner,cost)
            self.hidden=False
            self.counters=int(self.model['megamorph'])
            if tax_payment:
                self.pay(self.actor,[3,0,0,0,0,0])
            self.emit('refresh')
            # The special action has revealed the creature before any spell
            # which survives the trigger can resolve.
            self.source_traits(full=True)
            self.prop('source','plus1_counters',self.counters)
        elif pending:
            self.emit('assert_stack',value=1)
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_unused_choices',value=0)
        return allowed

    def parent_result(self,countered):
        if countered:
            self.leave('parent','exile' if self.effect=='exile' else 'graveyard')
        elif self.witness=='Lightning Bolt':
            self.leave('parent','graveyard')
            self.life[1-self.actor]-=3
        elif self.witness=='Divination':
            self.leave('parent','graveyard')
            for ref in ('library_0','library_1'):
                self.leave(ref,'hand')
        else:
            self.entities['parent']['zone']='battlefield'

    def cast_bounce(self):
        self.cast_spell('bounce','source')
        self.resolve()
        self.leave('bounce','graveyard')
        self.leave('source','hand')
        self.counters=0
        self.emit('bind_hand',ref='source',player=self.owner,value=self.csvid)
        self.prop('source','in_hand',1)
        self.receipts()

    def permission(self,actor,allowed,*,use=False,witness=None):
        name=self.witness if witness is None else witness
        fields=dict(player=actor,index=self.rows[name]['csvid'],value=int(allowed))
        if actor!=self.actor:
            fields['owner']=self.actor
        self.emit('can_play_exiled',**fields)
        if use:
            self.emit('play_exiled',**fields)

    def play_stolen(self):
        mode=self.mode
        if mode=='permission_persists':
            self.cast_bounce()
            # Persistence is checked on the next own turn, after an actual
            # opponent turn and with no source remaining on the battlefield.
            self.advance()
            self.advance()
        if mode=='wrong_actor':
            self.permission(self.foe,False,use=True)
            self.permission(self.owner,False,witness='Island')
            self.permission(self.owner,True)
            self.receipts()
            return
        if mode=='creature_offturn':
            self.permission(self.owner,False,use=True)
            self.receipts()
            self.advance()
        if mode=='play_replay':
            self.replay=len(self.steps)
        self.pools()
        self.permission(self.owner,True,use=True)
        # The actual permission callback runs the permitted cast; there is no
        # replacement cast from hand or newly seeded mana here.
        self.resolve()
        if self.mode in FREE_INSTANT_MODES:
            self.leave('parent','graveyard')
            # The controlled libraries and short graveyards give the free
            # caster one actual top-card draw, independently of exile owner.
            self.leave(f'draw_library_{self.owner}_0','hand')
        else:
            self.entities['parent'].update(controller=self.owner,zone='battlefield')
            self.bind('parent')
        self.permission(self.owner,False,use=True)
        self.receipts()

    def flying_combat(self):
        self.advance()
        self.advance()
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.entities['source']['tapped']=True
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        air=self.mode=='flying'
        self.emit('block',ref='ground',other='source',value=int(not air))
        if air:
            self.emit('block',ref='reach',other='source',value=1)
        self.emit('advance_combat')
        self.prop('source','blocked',1)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        if air:
            self.entities['reach']['damage']=self.stats('source')[0]
            self.leave('source','graveyard')
        else:
            self.entities['source']['damage']=1
            self.leave('ground','graveyard')
        self.receipts()

    def play(self):
        self.start()
        mode=self.mode
        self.source_cast(hidden=mode!='ordinary',allowed=mode!='short_hidden')
        if mode in ('ordinary','short_hidden'):
            return
        if mode=='hidden_ground':
            self.flying_combat()
            return
        pending=mode not in NO_PARENT
        if pending:
            self.parent_cast({'tax_paid':3,'tax_declined':4,'tax_short':2}.get(mode,0))
        tax_payment=(mode=='tax_paid') if mode in ('tax_paid','tax_declined') else None
        flipped=self.flip(pending,tax_payment)
        if pending:
            legal_type=self.effect!='typed' or self.witness in ('Lightning Bolt','Divination')
            self.parent_result(flipped and legal_type and not tax_payment and mode!='uncounterable')
        self.receipts()
        if mode=='flying':
            self.flying_combat()
        if mode=='bounce_reset':
            self.cast_bounce()
            self.source_cast()
        if self.effect=='exile':
            if mode in FREE_PLAY+('wrong_actor',):
                self.play_stolen()
            if mode=='uncounterable':
                self.permission(self.owner,False,use=True)
                self.receipts()


def expand(name,csvid,declaration,model,selected,rows,oracle):
    validate_witnesses(oracle,rows)
    if (not selected or len(set(selected))!=len(selected) or not set(selected)<=set(variants(model))
            or csvid!=profiles()['sources'][name]['registry']['csvid']):
        raise ValueError('unreserved pending Morph card or scenario')
    cases=[]
    for variant in selected:
        mode,owner=variant.rsplit('_',1)
        scene=PendingMorphScene(name,model,mode,int(owner),rows,oracle)
        scene.play()
        steps=scene.steps
        if scene.replay is not None:
            cut=scene.replay
            tail=steps[cut:]
            steps=steps[:cut]+[step('capture')]+tail+[step('record_state'),step('ai_roundtrip'),step('restore')]+deepcopy(tail)+[step('assert_recorded_state')]
        gaps=['Selecting a lower target beneath another simultaneously pending spell and multiplayer.']
        if model['effect']=='exile':
            gaps.append('Independent exile removal before first use and multiple physically distinct copies of one CSVID.')
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                      covered_clauses=declaration['oracle_text'].splitlines(),untested_components=gaps)
        cases.append(dict(name=f'PendingSpellMorphRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=scene.setup,steps=steps,_recipe=metadata))
    return cases
