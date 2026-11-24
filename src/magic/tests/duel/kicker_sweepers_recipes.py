"""Observe actual Kicker payments and printed sweeper or conditional destruction."""

from collections import Counter
from functools import lru_cache
import json
from pathlib import Path
import re

from library_search_recipes import wave6_exact


FAMILY = 'kicker_sweepers'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
KICKER = re.compile(r'Kicker ((?:\{(?:\d+|[BUGRW])\})+) \(You may pay an additional \1 as you cast this spell\.\)')
DAMAGE = re.compile(r'(?P<source>.+) deals 1 damage to each creature (?P<flight>with|without) flying and each player\. '
                    r'If this spell was kicked, it deals 4 damage to each creature (?P=flight) flying and each player instead\.')
MODELS = {
    ('Creatures target player controls get -1/-1 until end of turn. If this spell was kicked, those creatures get -2/-2 until end of turn instead.',):
        ('shrink', 'marsh_casualties', ()),
    ('Destroy target artifact if its mana value is 2 or less. If this spell was kicked, destroy that artifact if its mana value is 5 or less instead.',):
        ('artifact', 'overload', ()),
    ('Flying', 'When this creature enters, destroy all lands you control. If it was kicked, destroy all lands instead.'):
        ('lands', 'desolation_angel', ('Flying',)),
    ('When this creature enters, destroy all other creatures you control. If it was kicked, destroy all other creatures instead.',):
        ('creatures', 'desolation_giant', ()),
}
ARTIFACTS = {'threshold_two': 'Mind Stone', 'threshold_six': 'Dreamstone Hedron',
             'threshold_three': 'Jalum Tome', 'threshold_three_paid': 'Jalum Tome', 'optional_low': 'Mind Stone'}
BASICS = ('Island', 'Forest', 'Plains', 'Swamp')
ZONES = ('hand','battlefield','library','graveyard','exile')
SUPPORT = set(BASICS) | {'Grizzly Bears','Enormous Baloth','Wind Drake','Mahamoti Djinn','Suntail Hawk','Hill Giant',
                         'Mind Stone','Dreamstone Hedron','Jalum Tome','Gilded Lotus','Icy Manipulator','Counterspell'}
VANILLA = set(BASICS) | {'Grizzly Bears','Enormous Baloth','Wind Drake','Mahamoti Djinn','Suntail Hawk','Hill Giant'}
COMMON = ('paid','declined','base_only','fee_short','cast_short','countered','replay')
SPECIAL = {
    'damage': (),
    'shrink': ('self_target',),
    'artifact': ('threshold_two','threshold_six','threshold_three','threshold_three_paid','self_artifact','optional_low'),
    'lands': ('wrong_white',),
    'creatures': ('wrong_white',),
}
PAID = {'paid','threshold_six','threshold_three_paid','self_target','self_artifact','optional_low','countered','replay'}


@lru_cache(maxsize=1)
def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def classify(record):
    from recipes import cost_from_oracle

    lines = record['oracle_text'].splitlines()
    plain = tuple(lines[1:])
    kicker = KICKER.fullmatch(lines[0]) if lines else None
    damage = DAMAGE.fullmatch(plain[0]) if len(plain) == 1 else None
    if record['layout'] != 'normal' or record.get('selected_face') or record.get('card_faces') or not kicker:
        raise ValueError('whole normal exact Kicker Oracle record required')
    if damage and damage['source'] == record['name']:
        effect, keywords = 'damage', []
        flight = damage['flight'] == 'with'
    elif plain in MODELS:
        effect, _, keys = MODELS[plain]
        keywords, flight = list(keys), None
    else:
        raise ValueError('unreviewed complete Kicker sweeper effect')
    if set(record['keywords']) != {'Kicker', *keywords}:
        raise ValueError('Kicker sweeper keyword metadata differs from printed clauses')
    kind = record['type_line'].split(' — ')[0]
    if kind != {'lands':'Creature','creatures':'Creature','artifact':'Instant'}.get(effect,'Sorcery'):
        raise ValueError('unexpected printed Kicker sweeper source type')
    return dict(effect=effect,flight=flight,mana=cost_from_oracle(record),
                kicker_mana=cost_from_oracle({'mana_cost':kicker[1]}),keywords=keywords,
                stats=[int(record[k]) for k in ('power','toughness')] if 'Creature' in kind else [])


def handler(record):
    lines = tuple(record['oracle_text'].splitlines()[1:])
    if lines in MODELS:
        return 'manalinkeh_card_' + MODELS[lines][1]
    damage = DAMAGE.fullmatch(lines[0])
    if not damage or damage['source'] != record['name']:
        raise ValueError('unknown Kicker sweeper handler')
    return 'manalinkeh_card_' + {'with':'canopy_surge','without':'breath_of_darigaaz'}[damage['flight']]


def variants(shape):
    return [f'{mode}_{actor}' for mode in (*COMMON,*SPECIAL[shape['effect']]) for actor in (0,1)]


def spec(declaration, record):
    shape = classify(record)
    pinned = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (not pinned or not wave6_exact(pinned['oracle'],record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text']
            or not wave6_exact(declaration.get('params'),shape) or not isinstance(chosen,list) or not chosen
            or any(type(v) is not str for v in chosen)
            or len(set(chosen))!=len(chosen) or set(chosen)-set(variants(shape))):
        raise ValueError('exact printed source, kicker, and distinct variants required')
    return dict(shape,family=FAMILY)


def validate_witnesses(oracle, rows):
    pins = profiles()
    if pins['version']!=1 or set(pins['witnesses'])!=SUPPORT:
        raise ValueError('complete exact Kicker sweeper witness profiles required')
    for group in ('sources','witnesses'):
        for name,pin in pins[group].items():
            record,row=oracle[name],rows[name]
            if (not wave6_exact(pin,dict(oracle=record,registry=row)) or record['layout']!='normal'
                    or row['kind']!='card' or row['handler'] in ('NULL','0',None)
                    or (row['handler']=='card_dummy' and name not in VANILLA)):
                raise ValueError(f'changed exact sweeper record {name}')
            if group=='sources' and row['handler']!=handler(record):
                raise ValueError(f'changed sweeper implementation {name}')


def types(record):
    return record['type_line'].split(' — ')[0].split()


class Sweep:
    def __init__(self, record, shape, actor, mode, rows, oracle):
        self.source, self.shape, self.effect = record, shape, shape['effect']
        self.actor, self.enemy, self.mode = actor, 1-actor, mode
        self.paid = mode in PAID
        self.oracle, self.rows = oracle, rows
        self.setup, self.steps = [], []
        self.permanents = {}
        self.add('source',actor,record['name'],0)
        self.target_player = actor if mode in ('self_target','self_artifact') else self.enemy
        self.add_battlefield()
        if mode=='countered':
            self.add('counter',self.enemy,'Counterspell',0)
        for seat in (0,1):
            self.add(f'deck_{seat}_0',seat,'Island',2)
            self.add(f'deck_{seat}_1',seat,'Plains',2)

    def add(self, ref, seat, name, zone=1):
        from recipes import card

        self.setup.append(card(name,ref,seat,ZONES[zone]))

    def emit(self, op, ref='', other='', property_name='', *, zone=None, **fields):
        from recipes import step

        if ref:
            fields['ref']=ref
        if other:
            fields['other']=other
        if property_name:
            fields['property']=property_name
        if zone is not None:
            fields['zone']=ZONES[zone]
        self.steps.append(step(op,**fields))

    def observe(self, ref, property_name, value):
        self.emit('assert_card',ref,property_name=property_name,value=value)

    def mana(self, op, actor, pool):
        for color,value in enumerate(pool):
            self.emit(op,player=actor,value=value,color=color)

    def zone(self, name, actor, zone, amount):
        self.emit('assert_zone_multiplicity',player=actor,zone=zone,index=self.rows[name]['csvid'],value=amount)

    def permanent(self, seat, suffix, name):
        ref = f'p{seat}_{suffix}'
        self.add(ref,seat,name)
        if set(types(self.oracle[name])) & {'Land','Artifact'}:
            self.setup[-1]['state']=['tapped']
        self.permanents[ref]=(seat,name)
        return ref

    def add_battlefield(self):
        for seat in (0, 1):
            if self.effect == 'damage':
                self.permanent(seat, 'ground_small', 'Grizzly Bears')
                self.permanent(seat, 'ground_high', 'Enormous Baloth')
                self.permanent(seat, 'fly_small', 'Wind Drake')
                self.permanent(seat, 'fly_high', 'Mahamoti Djinn')
                self.permanent(seat, 'land', BASICS[seat])
            elif self.effect == 'shrink':
                self.permanent(seat, 'one', 'Suntail Hawk')
                self.permanent(seat, 'two', 'Grizzly Bears')
                self.permanent(seat, 'three', 'Hill Giant')
                self.permanent(seat, 'land', BASICS[seat])
            elif self.effect == 'artifact':
                if seat == self.target_player:
                    self.target = ARTIFACTS.get(self.mode, 'Gilded Lotus')
                    self.target_ref = self.permanent(seat, 'target', self.target)
                    self.permanent(seat, 'other_artifact', 'Icy Manipulator')
                else:
                    self.permanent(seat, 'other_artifact', 'Mind Stone')
                self.permanent(seat, 'creature', 'Grizzly Bears')
            else:
                self.permanent(seat, 'first_land', BASICS[seat])
                self.permanent(seat, 'second_land', BASICS[2+seat])
                self.permanent(seat, 'ground', 'Grizzly Bears')
                self.permanent(seat, 'flyer', 'Wind Drake')
                self.permanent(seat, 'artifact', 'Icy Manipulator')

    def exact_zones(self, casualties, *, resolved, source_live=False, counter_resolved=False):
        for seat in (0, 1):
            remaining = Counter(name for ref, (who, name) in self.permanents.items() if seat == who and ref not in casualties)
            dead = Counter(name for ref, (who, name) in self.permanents.items() if seat == who and ref in casualties)
            if resolved and seat == self.actor:
                (remaining if source_live else dead)[self.source['name']] += 1
            if counter_resolved and seat == self.enemy:
                dead['Counterspell'] += 1
            self.emit('assert_zone', player=seat, zone=1, value=remaining.total())
            self.emit('assert_zone', player=seat, zone=3, value=dead.total())
            self.emit('assert_zone', player=seat, zone=4, value=0)
            for name in sorted(remaining.keys() | dead.keys()):
                self.zone(name, seat, 1, remaining[name])
                self.zone(name, seat, 3, dead[name])
            self.emit('assert_zone', player=seat, zone=2, value=2)
            for i, name in enumerate(('Island','Plains')):
                self.emit('assert_zone_card', player=seat, zone=2, index=i, value=self.rows[name]['csvid'])

    def outcome(self, ref):
        seat, name = self.permanents[ref]
        record = self.oracle[name]
        kinds, keywords = types(record), record['keywords']
        damage = penalty = 0
        destroyed = False
        if self.effect == 'damage' and 'Creature' in kinds and ('Flying' in keywords) == self.shape['flight']:
            damage = 4 if self.paid else 1
        elif self.effect == 'shrink' and 'Creature' in kinds and seat == self.target_player:
            penalty = 2 if self.paid else 1
        elif self.effect == 'artifact' and ref == self.target_ref:
            destroyed = record['cmc'] <= (5 if self.paid else 2)
        elif self.effect == 'lands':
            destroyed = 'Land' in kinds and (self.paid or seat == self.actor)
        elif self.effect == 'creatures':
            destroyed = 'Creature' in kinds and (self.paid or seat == self.actor)
        if 'Creature' in kinds:
            destroyed = destroyed or int(record['toughness']) <= penalty + damage
        return destroyed, damage, penalty

    def survivor_cards(self, *, expired=False):
        for ref, (_, name) in self.permanents.items():
            dead, damage, penalty = self.outcome(ref)
            if dead:
                continue
            record = self.oracle[name]
            self.observe(ref, 'in_play', 1)
            if 'Creature' in types(record):
                self.observe(ref, 'damage', damage)
                self.observe(ref, 'power', int(record['power']) - (0 if expired else penalty))
                self.observe(ref, 'toughness', int(record['toughness']) - (0 if expired else penalty))

    def angel_flying(self):
        self.emit('advance_turn')
        self.emit('advance_turn')
        self.emit('begin_attackers')
        self.emit('attack', 'source', value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block', f'p{self.enemy}_ground', 'source', value=0)
        self.emit('block', f'p{self.enemy}_flyer', 'source', value=1)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.emit('assert_stack')
        self.emit('assert_choices_consumed')
        self.observe('source', 'damage', 2)
        self.zone('Wind Drake', self.enemy, 1, 0)
        self.zone('Wind Drake', self.enemy, 3, 1)
        self.emit('assert_life', player=self.enemy, value=20)

    def funding(self):
        base = self.shape['mana']
        fee = self.shape['kicker_mana']
        if self.mode == 'cast_short':
            short = list(base)
            short[next(index for index,amount in enumerate(short) if amount)] -= 1
            return short,short,False
        if self.mode in ('base_only','threshold_two'):
            return list(base),[0]*6,False
        supplied = list(fee)
        if self.mode == 'fee_short':
            supplied[next(index for index,amount in enumerate(supplied) if amount)] -= 1
        if self.mode == 'wrong_white':
            supplied[0],supplied[5] = supplied[5],0
        offer = self.mode not in ('fee_short','wrong_white')
        return [x+y for x,y in zip(base,supplied)], [0]*6 if self.paid else supplied,offer

    def prepare_and_cast(self):
        funding, remaining, offer = self.funding()
        self.emit('turn',player=self.actor)
        self.mana('set_mana',self.actor,funding)
        self.mana('set_mana',self.enemy,[0]*6)
        self.emit('refresh')
        self.exact_zones(set(),resolved=False)
        if self.mode!='cast_short':
            if self.effect=='shrink':
                self.emit('choose_player',player=self.actor,value=self.target_player)
            elif self.effect=='artifact':
                self.emit('choose_target',self.target_ref,player=self.actor)
            if offer:
                self.emit('choose_dialog',player=self.actor,value=int(not self.paid))
        self.emit('cast','source',value=int(self.mode!='cast_short'))
        if self.mode=='optional_low':
            # A pending answer measures whether the full optional payment
            # is actually offered even on a cheap artifact.
            self.emit('assert_unused_choices',value=0)
        else:
            self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=int(self.mode!='cast_short'))
        self.observe('source','in_play',0)
        self.exact_zones(set(),resolved=False)
        self.mana('assert_mana',self.actor,remaining)
        self.mana('assert_mana',self.enemy,[0]*6)
        return remaining

    def pause_and_counter(self):
        from recipes import cost_from_oracle

        self.emit('announce_spell','source')
        self.mana('set_mana',self.enemy,cost_from_oracle(self.oracle['Counterspell']))
        self.emit('refresh')
        self.emit('cast_response','counter',value=1)
        self.emit('assert_stack',value=2)
        self.emit('assert_choices_consumed')
        self.emit('resolve_response')
        self.emit('refresh')
        self.emit('assert_stack',value=0)
        self.emit('assert_choices_consumed')
        self.exact_zones(set(),resolved=True,counter_resolved=True)
        self.zone(self.source['name'],self.actor,0,0)
        self.zone('Counterspell',self.enemy,0,0)
        for seat in (0,1):
            self.mana('assert_mana',seat,[0]*6)
            self.emit('assert_life',player=seat,value=20)
        for ref,(_,name) in self.permanents.items():
            self.observe(ref,'in_play',1)
            if 'Creature' in types(self.oracle[name]):
                self.observe(ref,'damage',0)
                self.observe(ref,'power',int(self.oracle[name]['power']))
                self.observe(ref,'toughness',int(self.oracle[name]['toughness']))

    def run(self):
        remaining = self.prepare_and_cast()
        if self.mode=='cast_short':
            self.observe('source','in_hand',1)
            for seat in (0,1):
                self.emit('assert_life',player=seat,value=20)
            return
        if self.mode=='countered':
            self.pause_and_counter()
            return
        self.emit('resolve')
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack')
        is_creature = self.effect in ('lands','creatures')
        self.zone(self.source['name'],self.actor,0,0)
        if is_creature:
            self.observe('source','in_play',1)
            for prop in ('power','toughness'):
                self.observe('source',prop,int(self.source[prop]))
        casualties = {ref for ref in self.permanents if self.outcome(ref)[0]}
        self.exact_zones(casualties,resolved=True,source_live=is_creature)
        self.survivor_cards()
        for seat in (0,1):
            self.emit('assert_life',player=seat,value=20-((4 if self.paid else 1) if self.effect=='damage' else 0))
        self.mana('assert_mana',self.actor,remaining)
        self.mana('assert_mana',self.enemy,[0]*6)
        if self.effect=='shrink':
            self.emit('cleanup')
            self.emit('refresh')
            self.survivor_cards(expired=True)
        if self.effect=='lands':
            self.angel_flying()


def expand(name, csvid, declaration, shape, selected, rows, oracle):
    from recipes import step

    cases = []
    for label in selected:
        mode, actor = label.rsplit('_',1)
        board = Sweep(oracle[name],shape,int(actor),mode,rows,oracle)
        board.run()
        if mode=='replay':
            body = board.steps
            board.steps = [step('capture')] + body + [step('record_state'),step('restore'),step('ai_roundtrip')] + body + [
                step('assert_recorded_state')]
        cases.append(dict(name=f'KickerSweepersRecipe_{csvid}_{label}',card=name,fixture='empty_main',seed=2298,
                          setup=board.setup,steps=board.steps,_recipe=dict(family=FAMILY,version=2,variant=label,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],untested_components=[])))
    return cases
