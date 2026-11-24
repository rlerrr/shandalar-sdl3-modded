"""Whole-card spell protection grants through real counter attempts."""

from dataclasses import dataclass
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from uncounterable_creature_casts_recipes import DECK, INSTANT, NORMAL, UNCOUNTERABLE, UncounterableBoard


FAMILY = 'uncounterable_spell_permissions'
PROFILE = Path(__file__).parent / 'data/uncounterable_spell_permissions_wave8_profiles.json'
DECREE = 'Decree of Silence'
SLIVER = 'Plated Sliver'
CHANGELING = 'Nameless Inversion'
VANILLA_FOUR = 'Durkwood Boars'
VANILLA_FIVE = 'Spined Wurm'
VANILLA_SIX = 'Craw Wurm'
ARTIFACT = 'Darksteel Relic'
FLASH = 'Ambush Viper'
SORCERY = 'Divination'
FLASHBACK = 'Think Twice'
ALLY_CLAUSE = "Creature spells you control can't be countered."
MATCHES = {
    ('Creature — Sliver','{3}{G}','2','2',(),UNCOUNTERABLE+"\nSliver spells can't be countered."): 'slivers',
    ('Creature — Beast','{1}{R}{G}{G}','5','5',(),UNCOUNTERABLE+"\nCreature spells you control with power 5 or greater can't be countered."): 'power',
    ('Legendary Creature — Human Warrior','{2}{G}{U}{R}','6','6',('Flash',),
     'Flash\n'+UNCOUNTERABLE+'\n'+ALLY_CLAUSE+'\nOther creatures you control have trample.'): 'ally_flash',
    ('Creature — Sphinx','{5}{U}{U}','5','5',('Flying','Hexproof'),UNCOUNTERABLE+
     "\nFlying\nHexproof (This creature can't be the target of spells or abilities your opponents control.)"
     "\nInstant and sorcery spells you control can't be countered."): 'spells',
    ('Creature — Cat Snake','{1}{G}{G}','4','3',(),UNCOUNTERABLE+'\n'+ALLY_CLAUSE): 'ally',
}
COMMON = ('intrinsic','intrinsic_replay','intrinsic_decree','source_absent','source_removed','source_entered')
RIDERS = {'slivers':(), 'power':(), 'ally':(), 'spells':('own_spell','foe_spell','own_ability','foe_ability',
                'flying','reach','decree_flashback'), 'ally_flash':('flash_response','trample_own','trample_self','trample_enemy')}
STATIC = {NORMAL,VANILLA_FOUR,VANILLA_FIVE,VANILLA_SIX,'Hill Giant','Giant Spider'}
WITNESSES = set(DECK)|STATIC|{DECREE,SLIVER,CHANGELING,ARTIFACT,FLASH,SORCERY,FLASHBACK,INSTANT,
                           'Counterspell','Exclude','Unsummon','Giant Growth','Master Decoy'}


@dataclass(frozen=True)
class Subject:
    name: str
    opponent: bool = False
    off_turn: bool = False


ALLY_SUBJECTS = {'creature':Subject(NORMAL),'opposing_creature':Subject(NORMAL,opponent=True),
                 'instant':Subject(INSTANT),'artifact':Subject(ARTIFACT),'flash':Subject(FLASH,off_turn=True)}
MATRIX = {
    'slivers': {'plated':Subject(SLIVER),'opposing_plated':Subject(SLIVER,opponent=True),
                'kindred':Subject(CHANGELING),'opposing_kindred':Subject(CHANGELING,opponent=True),
                'creature':Subject(NORMAL),'instant':Subject(INSTANT)},
    'power': {'power_five':Subject(VANILLA_FIVE),'power_four':Subject(VANILLA_FOUR),'power_six':Subject(VANILLA_SIX),
              'opposing_five':Subject(VANILLA_FIVE,opponent=True),'instant':Subject(INSTANT)},
    'ally_flash': ALLY_SUBJECTS,
    'spells': {'instant':Subject(INSTANT),'sorcery':Subject(SORCERY),
               'opposing_instant':Subject(INSTANT,opponent=True),'opposing_sorcery':Subject(SORCERY,opponent=True),
               'creature':Subject(NORMAL),'flash':Subject(FLASH,off_turn=True),'artifact':Subject(ARTIFACT)},
    'ally': ALLY_SUBJECTS,
}


def profile():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    key = (record['type_line'],record['mana_cost'],record.get('power'),record.get('toughness'),
           tuple(record['keywords']),record['oracle_text'])
    if record.get('layout')!='normal' or record.get('selected_face') or record.get('card_faces') or key not in MATCHES:
        raise ValueError('whole uncounterable spell grant and printed riders required')
    return dict(model=MATCHES[key],cost=cost_from_oracle(record))


def variants(params):
    model=params['model']
    scopes=[kind+'_'+subject for kind in ('paid','decree') for subject in MATRIX[model]]
    return [f'{mode}_{seat}' for seat in (0,1) for mode in (*COMMON,*RIDERS[model],*scopes)]


def spec(declaration,record):
    params=parameters(record)
    chosen=declaration.get('variants')
    if (declaration.get('family')!=FAMILY or type(declaration.get('version')) is not int or declaration['version']!=2
            or not exact(profile()['sources'].get(record['name'],{}).get('oracle'),record)
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),params)):
        raise ValueError('exact source and complete printed grant required')
    if (not isinstance(chosen,list) or not chosen or any(type(mode) is not str for mode in chosen)
            or len(chosen)!=len(set(chosen)) or set(chosen)-set(variants(params))):
        raise ValueError('distinct reviewed spell permission variants required')
    return dict(params,family=FAMILY)


def validate_witnesses(oracle,rows):
    data=profile()
    if set(data['witnesses'])!=WITNESSES or len(data['sources'])!=len(MATCHES):
        raise ValueError('complete spell protection witness inventory required')
    for name,pin in (data['sources']|data['witnesses']).items():
        row=rows[name]
        if row['kind']!='card' or not exact(row,pin['registry']) or not exact(oracle[name],pin['oracle']):
            raise ValueError(f'pinned Oracle or registry drift for {name}')
        if name not in STATIC and row['handler'] in ('card_dummy','0','NULL',None):
            raise ValueError(f'expected production action witness for {name}')
    threshold=(VANILLA_FOUR,VANILLA_FIVE,VANILLA_SIX)
    if ([int(oracle[name]['power']) for name in threshold]!=[4,5,6]
            or oracle[VANILLA_FOUR]['mana_cost']!=oracle[VANILLA_FIVE]['mana_cost']):
        raise ValueError('independent printed power boundary required')
    if ('Flash' not in oracle[FLASH]['keywords'] or oracle[FLASH]['type_line']!='Creature — Snake'
            or oracle[DECREE]['oracle_text'].splitlines()[0]!=
               'Whenever an opponent casts a spell, counter that spell and put a depletion counter on this enchantment. If there are three or more depletion counters on this enchantment, sacrifice it.'):
        raise ValueError('exact non-targeted counter and Flash creature witnesses required')


def components(record):
    return record['type_line'].split(' — ',1)[0].split()


def grants(model,record,caster_is_controller):
    types=components(record)
    if model=='slivers':
        return ('Sliver' in record['type_line'].partition(' — ')[2].split() or 'Changeling' in record['keywords'])
    if not caster_is_controller:
        return False
    if model=='spells':
        return bool({'Instant','Sorcery'}.intersection(types))
    if 'Creature' not in types:
        return False
    return model!='power' or int(record['power'])>=5


class PermissionBoard(UncounterableBoard):
    def __init__(self,name,params,variant,rows,oracle):
        self.mode,seat=variant.rsplit('_',1)
        self.owner,self.enemy=int(seat),1-int(seat)
        self.name,self.model,self.rows,self.oracle=name,params['model'],rows,oracle
        self.setup,self.steps,self.entities=[],[],{}
        self.zones={key:[[],[]] for key in ('hand','battlefield','graveyard','library','exile')}
        self.mana,self.life=[[0]*6,[0]*6],[20,20]
        self.loyalty=self.replay=None
        self.active=self.owner
        self.actor=self.owner
        self.intrinsic=self.mode.startswith('intrinsic')
        self.nontargeted=self.mode.startswith('decree_') or self.mode=='intrinsic_decree'
        is_hand=self.intrinsic or self.mode in ('source_absent','source_entered','flash_response')
        self.add(name,'source',self.owner,'hand' if is_hand else 'battlefield')
        self.subject=None
        if self.intrinsic or self.mode.startswith(('paid_','decree_','source_')):
            self.setup_contest()
        else:
            self.setup_rider()
        for who in (0,1):
            for index,card in enumerate(DECK):
                self.add(card,f'library_{who}_{index}',who,'library')
        self.names={entry['name'] for entry in self.entities.values()}|set(DECK)
        if self.subject:
            self.names.add(self.subject.name)

    def setup_contest(self):
        if self.intrinsic:
            self.subject=Subject(self.name)
        elif self.mode=='decree_flashback':
            self.subject=Subject(FLASHBACK)
        elif self.mode.startswith('source_'):
            self.subject=next(iter(MATRIX[self.model].values()))
        else:
            self.subject=MATRIX[self.model][self.mode.split('_',1)[1]]
        if self.subject.opponent:
            self.actor=self.enemy
        self.active=1-self.actor if self.subject.off_turn else self.actor
        self.spell_ref='source' if self.intrinsic else 'subject'
        if not self.intrinsic:
            self.add(self.subject.name,self.spell_ref,self.actor,'graveyard' if self.mode=='decree_flashback' else 'hand')
        self.responder=1-self.actor
        if self.nontargeted:
            self.add(DECREE,'decree',self.responder)
        else:
            creature='Creature' in components(self.oracle[self.subject.name])
            self.counter='Exclude' if creature and self.mode!='intrinsic' else 'Counterspell'
            self.add(self.counter,'counter',self.responder,'hand')
        if self.subject.name==CHANGELING:
            self.add('Hill Giant','victim',self.responder)
        if self.mode=='source_removed':
            self.add('Unsummon','bounce',self.owner,'hand')

    def setup_rider(self):
        if self.mode=='flash_response':
            self.active=self.enemy
            self.add(INSTANT,'parent',self.enemy,'hand')
        elif self.mode.endswith(('_spell','_ability')):
            who=self.owner if self.mode.startswith('own_') else self.enemy
            if self.mode.endswith('_spell'):
                self.add('Giant Growth','targeter',who,'hand')
            else:
                self.add('Master Decoy','targeter',who)
        elif self.mode in ('flying','reach'):
            self.add('Giant Spider' if self.mode=='reach' else NORMAL,'blocker',self.enemy)
        else:
            self.active=self.enemy if self.mode=='trample_enemy' else self.owner
            if self.mode!='trample_self':
                self.add('Hill Giant','attacker',self.active)
            self.add(NORMAL,'blocker',1-self.active)

    def observe_decree(self):
        self.emit('assert_counter',ref='decree',counter_kind='depletion',value=1)

    def fee(self,ref):
        name=self.entities[ref]['name']
        if name==ARTIFACT and self.oracle[name]['mana_cost']=='{0}':
            return [0]*6
        return super().fee(ref)

    def contest(self):
        if self.mode=='source_removed':
            self.start('bounce',target='source')
            self.resolve('bounce','graveyard')
            self.move('source','hand')
            self.emit('bind_hand',player=self.owner,value=self.rows[self.name]['csvid'],ref='source')
        elif self.mode=='source_entered':
            self.start('source')
            self.resolve('source','battlefield')
        if self.mode in ('source_removed','source_entered'):
            self.receipt()

        if self.mode=='decree_flashback':
            self.flashback_contest()
            return
        self.start(self.spell_ref,target='victim' if self.subject.name==CHANGELING else None)
        self.emit('announce_spell',ref=self.spell_ref)
        active_grant=self.entities['source']['zone']=='battlefield'
        protection=self.intrinsic or (active_grant and grants(self.model,self.oracle[self.subject.name],self.actor==self.owner))
        if self.nontargeted:
            self.emit('resolve')
            self.observe_decree()
        elif active_grant:
            # Actual paid targeting in a snapshot. Decree independently tests the counter outcome.
            self.emit('capture')
            self.emit('record_state')
            mana=[pool.copy() for pool in self.mana]
            self.start('counter',response=True)
            self.emit('restore')
            self.emit('assert_recorded_state')
            self.mana=mana
            self.emit('resolve')
        else:
            self.start('counter',response=True)
            if self.mode=='intrinsic_replay':
                self.replay=len(self.steps)
            self.emit('resolve_response')
            self.move('counter','graveyard')
            if self.counter=='Exclude':
                self.draw(self.responder)
        resolves_without_counter=active_grant and not self.nontargeted
        self.move_subject(resolved=resolves_without_counter or protection)
        self.emit('refresh')

    def flashback_contest(self):
        self.fund(self.actor,[2,0,1,0,0,0])
        self.emit('prepare_graveyard_abilities')
        self.emit('can_cast_graveyard',ref='subject',value=1)
        self.emit('cast_graveyard',ref='subject',value=1)
        self.mana[self.actor]=[0]*6
        self.zones['graveyard'][self.actor].remove(FLASHBACK)
        self.zones['exile'][self.actor].append(FLASHBACK)
        self.draw(self.actor)
        self.observe_decree()

    def move_subject(self,*,resolved):
        card=self.subject.name
        is_spell=bool({'Instant','Sorcery'}.intersection(components(self.oracle[card])))
        self.move(self.spell_ref,'graveyard' if is_spell or not resolved else 'battlefield')
        if not resolved:
            return
        if card in (INSTANT,SORCERY):
            for _ in range(2 if card==SORCERY else 1):
                self.draw(self.actor)
        elif card==SLIVER:
            self.observe('subject','in_play',1)
            self.observe('subject','power',1)
            self.observe('subject','toughness',2)
            if self.entities['source']['zone']=='battlefield':
                self.entities['source']['stats']=(2,3)
        elif card==CHANGELING:
            self.move('victim','graveyard')

    def targeting_rider(self):
        who=self.entities['targeter']['player']
        allowed=who==self.owner
        if self.mode.endswith('_spell'):
            self.emit('can_target_creature',ref='targeter',other='source',value=int(allowed))
            self.emit('can_cast',ref='targeter',value=int(allowed))
            if allowed:
                self.start('targeter',target='source')
                self.resolve('targeter','graveyard')
                self.entities['source']['stats']=(8,8)
                self.receipt()
                self.emit('cleanup')
                self.emit('refresh')
                self.entities['source']['stats']=(5,5)
            return
        self.fund(who,[0,0,0,0,0,1])
        self.emit('probe_activation_target',ref='targeter',other='source',index=-1,value=int(allowed))
        if allowed:
            self.emit('choose_target',player=who,ref='source')
            self.emit('activate',ref='targeter',value=1)
            self.mana[who]=[0]*6
            self.observe('targeter','tapped',1)
            self.observe('source','tapped',0)
            self.emit('assert_stack',value=1)
            self.emit('resolve')
            self.observe('source','tapped',1)

    def combat_rider(self):
        attacker='attacker' if 'attacker' in self.entities else 'source'
        blocked=self.mode!='flying'
        self.emit('begin_attackers')
        self.emit('attack',ref=attacker,value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other=attacker,value=int(blocked))
        self.emit('advance_combat')
        self.observe(attacker,'blocked',blocked)
        self.emit('combat_damage')
        self.emit('end_combat')
        attack=self.entities[attacker]['stats'][0]
        if blocked:
            defend,toughness=self.entities['blocker']['stats']
            self.combat_hit(attacker,defend)
            self.combat_hit('blocker',attack)
            if self.mode=='trample_own':
                self.life[1-self.active]-=max(0,attack-toughness)
        else:
            self.life[1-self.active]-=attack

    def actions(self):
        from recipes import step

        self.emit('turn',player=self.active,value=0)
        self.fund(self.actor,[0]*6)
        if self.subject:
            self.contest()
        elif self.mode=='flash_response':
            self.flash_response()
        elif self.mode.endswith(('_spell','_ability')):
            self.targeting_rider()
        else:
            self.combat_rider()
        self.receipt()
        if self.replay is not None:
            tail=self.steps[self.replay:]
            self.steps[self.replay:]=[step('capture'),*tail,step('record_state'),step('ai_roundtrip'),
                                     step('restore'),*tail,step('assert_recorded_state')]


def expand(name,csvid,declaration,params,chosen,rows,oracle):
    validate_witnesses(oracle,rows)
    if rows[name]['csvid']!=csvid or not chosen or set(chosen)-set(variants(params)):
        raise ValueError('unbound spell permission source or variant')
    cases=[]
    for variant in chosen:
        board=PermissionBoard(name,params,variant,rows,oracle)
        board.actions()
        metadata=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                      untested_components=['grant enters/leaves with protected spell and counter both pending',
                                           'variable characteristic-defining power on the stack','multiplayer and replacement effects'])
        cases.append(dict(name=f'UncounterableSpellPermissionRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=board.setup,steps=board.steps,_recipe=metadata))
    return cases
