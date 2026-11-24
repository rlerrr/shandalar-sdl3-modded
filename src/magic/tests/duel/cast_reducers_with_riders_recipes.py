"""Subtype spell reductions paired with their printed activated, static or triggered riders."""

import copy
import json
import re
from pathlib import Path

from block_restriction_spells_recipes import _World as _SpellWorld


FAMILY = 'cast_reducers_with_riders'
PROFILE = Path(__file__).parent / 'data/cast_reducers_with_riders_wave8_profiles.json'
MODEL = {
    'Beast': ('regen', 'krosan_warchief', 1, [], ['Regenerate'], '{1}{G}: Regenerate target Beast.'),
    'Arcane': ('anthem', 'long_forgotten_gohei', 1, [], [], 'Spirit creatures you control get +1/+1.'),
    'Sphinx': ('piles', 'unesh_criosphinx_sovereign', 2, ['Flying'], ['Flying'],
               'Whenever Unesh or another Sphinx you control enters, reveal the top four cards of your library. '
               'An opponent separates those cards into two piles. Put one pile into your hand and the other into your graveyard.'),
    'Aura': ('heroic', 'hero_of_iroas', 1, [], ['Heroic'],
             'Heroic — Whenever you cast a spell that targets this creature, put a +1/+1 counter on this creature.'),
}
DISCOUNT = re.compile(r'(Beast|Arcane|Sphinx|Aura) spells you cast cost \{([12])\} less to cast\.')
COMMON = ('match', 'surplus', 'short', 'opponent', 'nonmatch', 'removed', 'source_short', 'replay')
EXTRA = {
    'regen': ('save_self', 'save_own', 'save_enemy', 'proactive', 'forbidden_regeneration', 'wrong_regen_cost', 'wrong_type'),
    'anthem': ('spirit_late', 'floor', 'countered'),
    'piles': ('self_piles', 'partial_piles', 'other_piles', 'countered', 'opposing_piles', 'nonmatch_piles', 'reanimate_piles',
              'floor', 'fly_ground', 'fly_reach'),
    'heroic': ('countered', 'offcolor_aura', 'other_target', 'opponent_target', 'instant_heroic', 'floor', 'counter_persists'),
}
MATCH = dict(Beast='Rumbling Baloth', Arcane='Glacial Ray', Sphinx='Sphinx of Magosi', Aura='Pacifism')
NONMATCH = dict(Beast='Hill Giant', Arcane='Lightning Strike', Sphinx='Wind Drake', Aura='Levitation')
WITNESSES = set(MATCH.values()) | set(NONMATCH.values()) | {
    'Lava Spike', 'Kami of Old Stone', 'Vexing Sphinx', 'Encrust', 'Flight', 'Giant Growth',
    'Grizzly Bears', 'Giant Spider', 'Murder', 'Terminate', 'Boomerang', 'Counterspell', 'Island', 'Forest', 'Zombify',
}
DUMMIES = {'Rumbling Baloth', 'Grizzly Bears', 'Giant Spider', 'Hill Giant', 'Kami of Old Stone', 'Wind Drake'}


def profiles():
    return json.loads(PROFILE.read_bytes())


def classify(record):
    from recipes import cost_from_oracle

    if record.get('layout') != 'normal' or record.get('card_faces') or record.get('selected_face'):
        raise ValueError('whole normal source required')
    lines = record['oracle_text'].splitlines()
    passives = [lines.pop(0)] if lines and lines[0] == 'Flying' else []
    if len(lines) != 2 or not (match := DISCOUNT.fullmatch(lines[0])):
        raise ValueError('complete subtype reducer and rider required')
    subtype, reduction = match[1], int(match[2])
    mode, _, amount, expected_passives, keys, rider = MODEL[subtype]
    types = record['type_line'].partition(' — ')[0].split()
    creature = 'Creature' in types
    if (reduction != amount or lines[1] != rider or passives != expected_passives or sorted(record['keywords']) != sorted(keys)
            or creature != (mode != 'anthem') or not creature and types != ['Artifact']):
        raise ValueError('unsupported whole subtype-rider combination')
    if creature and any(not re.fullmatch('[0-9]+',record.get(field,'')) for field in ('power','toughness')):
        raise ValueError('expected fixed creature stats')
    return dict(subtype=subtype, reduction=reduction, mode=mode, cost=cost_from_oracle(record), keywords=passives,
                regen_cost=cost_from_oracle(dict(mana_cost=rider.split(':')[0])) if mode == 'regen' else [])


def handler(record):
    model = classify(record)
    return 'manalinkeh_card_' + MODEL[model['subtype']][1]


def variants(params):
    return [f'{scene}_{seat}' for scene in (*COMMON,*EXTRA[params['mode']]) for seat in (0,1)]


def spec(declaration, record):
    p = classify(record)
    source = profiles()['sources'].get(record['name'])
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not source or source['oracle'] != record
            or declaration.get('oracle_text') != record['oracle_text']
            or json.dumps(declaration.get('params'), sort_keys=True) != json.dumps(p, sort_keys=True)):
        raise ValueError('exact whole Oracle reducer with rider required')
    if (type(chosen) is not list or not chosen or any(type(name) is not str for name in chosen)
            or len(set(chosen)) != len(chosen) or not set(chosen) <= set(variants(p))):
        raise ValueError('distinct approved reducer scenarios required')
    return dict(p, family=FAMILY, effect=FAMILY)


def has_subtype(record, subtype):
    return subtype in record['type_line'].partition(' — ')[2].split()


def validate_witnesses(oracle, registry):
    p = profiles()
    if p.get('version') != 1 or set(p['witnesses']) != WITNESSES:
        raise ValueError('complete independently pinned witnesses required')
    for group in ('sources','witnesses'):
        for name, expected in p[group].items():
            row, record = registry.get(name,{}), oracle.get(name,{})
            native = group == 'witnesses' and name in DUMMIES and expected.get('reviewed_native_characteristics')
            if (expected['registry'] != row or expected['oracle'] != record or row.get('kind') != 'card'
                    or record.get('layout') != 'normal' or row.get('handler') in (None,'NULL') or row['handler'] == 'card_dummy' and not native):
                raise ValueError(f'changed reducer or witness: {name}')
            if group == 'sources' and row['handler'] != handler(record):
                raise ValueError(f'changed source callback: {name}')
    for subtype in MODEL:
        if not has_subtype(oracle[MATCH[subtype]],subtype) or has_subtype(oracle[NONMATCH[subtype]],subtype):
            raise ValueError('actual qualifying and nonqualifying spell witnesses required')


class _World(_SpellWorld):
    def __init__(self, name, p, owner, registry, oracle, scene):
        super().__init__(name,p,owner,registry,oracle)
        self.scene = scene
        self.heroic = 0
        self.growth = 0

    def active(self):
        return self.cards['source'][2] == 'battlefield'

    def emit(self, op, **fields):
        if (self.p['mode'] == 'heroic' and op == 'assert_card' and fields.get('ref') == 'source'
                and fields.get('property') == 'plus1_counters'):
            fields['value'] = self.heroic
        super().emit(op,**fields)

    def receipts(self):
        for ref, creature in self.entities.items():
            if creature['zone'] != 'battlefield':
                continue
            record = self.o[creature['name']]
            bonus = 0
            if self.active() and self.p['mode'] == 'anthem' and creature['player'] == self.owner and has_subtype(record,'Spirit'):
                bonus = 1
            if ref == 'source' and self.p['mode'] == 'heroic':
                bonus = self.heroic + self.growth
            for prop in ('power','toughness'):
                creature[prop] = int(record[prop]) + bonus
        super().receipts()

    def wallets(self):
        for seat in (0,1):
            for color, amount in enumerate(self.mana[seat]):
                self.emit('assert_mana',player=seat,color=color,value=amount)

    def fee(self, ref):
        from recipes import cost_from_oracle

        nm,actor,_ = self.cards[ref]
        printed = cost_from_oracle(self.o[nm])
        actual = list(printed)
        if self.active() and actor == self.owner and has_subtype(self.o[nm],self.p['subtype']):
            actual[0] = max(0, actual[0]-self.p['reduction'])
        return printed, actual

    def paid(self, ref, *, allowed=True, budget=None, target=None, player_target=None, zone_target=None):
        _, actor, zone = self.cards[ref]
        if zone != 'hand':
            raise ValueError('paid cast needs the actual hand card')
        _, fee = self.fee(ref)
        pool = list(fee if budget is None else budget)
        self.pool(actor,pool)
        self.emit('refresh')
        if allowed and target:
            self.emit('choose_target',ref=target,player=actor)
        elif allowed and player_target is not None:
            self.emit('choose_player',player=actor,value=player_target)
        elif allowed and zone_target:
            self.emit('choose_zone_card',player=actor,value=self.rows[zone_target]['csvid'])
        self.emit('cast',ref=ref,value=int(allowed))
        self.emit('assert_stack',value=int(allowed))
        self.emit('assert_unused_choices',value=0)
        self.observe(ref,'in_hand',not allowed)
        if allowed:
            self.mana[actor] = [p-f for p,f in zip(pool,fee)]
        self.wallets()

    def choose_piles(self):
        # The chosen fixtures constrain partition without relying on unequal AI valuations.
        deck = self.zones['library'][self.owner]
        count = min(4,len(deck))
        if not count:
            return
        if count == 1:
            choices, selected = [-1], 1
        elif self.owner == 0:
            choices, selected = [-1], (count+1)//2
        else:
            choices = [*range((count+1)//2,count),-1,-1]
            selected = (count+1)//2
        for choice in choices:
            self.emit('choose_zone',player=0,value=choice)
        visible, leftover = deck[:count], deck[count:]
        self.zones['library'][self.owner] = leftover
        self.zones['hand'][self.owner].extend(visible[:selected])
        self.zones['graveyard'][self.owner].extend(visible[selected:])

    def settle(self, ref, zone):
        self.finish(ref,zone)
        self.emit('assert_unused_choices',value=0)
        self.receipts()

    def next_main(self):
        self.growth = 0
        super().next_turn()

    def regenerate(self):
        scene, p = self.scene, self.p
        target = 'source' if scene == 'save_self' else 'recipient'
        cost = list(p['regen_cost'])
        if scene == 'wrong_regen_cost':
            cost[3] -= 1
            cost[0] += 1
        saves = scene not in ('forbidden_regeneration','wrong_regen_cost','wrong_type')
        if self.owner == 1 and saves and scene != 'proactive':
            # This source's legacy seat-1 response availability is postcombat.
            for op in ('begin_attackers','finish_attackers','begin_blockers','advance_combat','combat_damage','end_combat'):
                self.emit(op)
        self.pool(self.owner,cost)
        self.emit('refresh')
        if scene == 'proactive':
            self.emit('can_activate',ref='source',value=1)
            self.receipts()
            return
        self.paid('removal',target=target)
        self.emit('choose_regeneration',ref='source')
        if saves:
            self.emit('choose_target',ref=target,player=self.owner)
        self.finish('removal','graveyard')
        self.emit('assert_unused_choices',value=int(not saves))
        if saves:
            self.mana[self.owner] = [0]*6
            self.entities[target].update(tapped=True,damage=0,attacking=False)
        else:
            self.move(target,'graveyard')
        self.receipts()

    def flying(self):
        self.next_main()
        self.next_main()
        reaches = self.scene == 'fly_reach'
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='blocker',other='source',value=int(reaches))
        self.observe('blocker','blocking_count',int(reaches))
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.entities['source']['tapped'] = True
        if reaches:
            self.entities['source']['damage'] = 2
            self.move('blocker','graveyard')
        else:
            self.life[self.foe] -= self.entities['source']['power']
        self.receipts()

    def reanimate(self):
        self.paid('revival',zone_target='Sphinx of Magosi')
        self.choose_piles()
        self.finish('revival','graveyard')
        self.move('revived','battlefield')
        self.emit('bind_permanent_copy',ref='revived',player=self.owner,index=0,value=self.rows['Sphinx of Magosi']['csvid'])
        self.emit('assert_unused_choices',value=0)
        self.receipts()


def fixture(w):
    p, scene, owner, foe = w.p, w.scene, w.owner, w.foe
    mode, subtype = p['mode'],p['subtype']
    w.add(w.name,'source',owner,'hand')
    if scene == 'removed':
        w.add('Boomerang','reply',foe,'hand')
    if scene == 'countered':
        w.add('Counterspell','reply',foe,'hand')
    if mode == 'anthem':
        w.add('Kami of Old Stone','spirit_own',owner)
        w.add('Kami of Old Stone','spirit_foe',foe)
        w.add('Grizzly Bears','unboosted',owner)
    if mode == 'heroic':
        w.add('Grizzly Bears','other',owner)
        if scene == 'counter_persists':
            w.add('Boomerang','bounce_aura',foe,'hand')
    if mode == 'regen' and scene in EXTRA[mode]:
        if scene != 'save_self':
            w.add('Grizzly Bears' if scene == 'wrong_type' else 'Rumbling Baloth','recipient',foe if scene == 'save_enemy' else owner)
        w.add('Terminate' if scene == 'forbidden_regeneration' else 'Murder','removal',foe,'hand')
        return
    if mode == 'piles':
        if scene in ('self_piles','partial_piles','other_piles','countered','opposing_piles','nonmatch_piles','reanimate_piles','replay'):
            libraries = ['Island'] * (3 if scene == 'partial_piles' else 4)
            if scene in ('other_piles','countered','opposing_piles','nonmatch_piles','reanimate_piles'):
                libraries.append('Forest')
            for index, name in enumerate(libraries):
                w.add(name,f'library_{index}',owner,'library')
        if scene.startswith('fly_'):
            w.add('Giant Spider' if scene == 'fly_reach' else 'Hill Giant','blocker',foe)
            return
        if scene == 'reanimate_piles':
            w.add('Zombify','revival',owner,'hand')
            w.add('Sphinx of Magosi','revived',owner,'graveyard')
            return
        if scene in ('self_piles','partial_piles'):
            return
    if scene == 'source_short':
        return
    member = MATCH[subtype]
    if scene in ('nonmatch','nonmatch_piles'):
        member = NONMATCH[subtype]
    options = {
        ('anthem','spirit_late'): 'Kami of Old Stone', ('anthem','floor'):'Lava Spike',
        ('piles','floor'): 'Vexing Sphinx', ('heroic','floor'):'Flight',
        ('heroic','offcolor_aura'):'Encrust', ('heroic','instant_heroic'):'Giant Growth',
    }
    member = options.get((mode,scene),member)
    actor = foe if scene in ('opponent','opposing_piles','opponent_target') else owner
    w.add(member,'member',actor,'hand')


def member_episode(w):
    scene,mode,owner = w.scene,w.p['mode'],w.owner
    if scene in ('opponent','opposing_piles','opponent_target'):
        w.next_main()
    if scene == 'removed':
        w.paid('reply',target='source')
        w.finish('reply','graveyard')
        w.move('source','hand')
        w.receipts()
    printed, fee = w.fee('member')
    if scene in ('short','opponent','nonmatch','removed','opposing_piles','nonmatch_piles','opponent_target','spirit_late','floor'):
        low = list(fee)
        low[next(i for i,count in enumerate(low) if count)] -= 1
        w.paid('member',allowed=False,budget=low)
        w.receipts()
        if scene == 'short':
            return
    member, actor, _ = w.cards['member']
    record = w.o[member]
    target = None
    if mode == 'heroic' and (has_subtype(record,'Aura') or member == 'Giant Growth'):
        target = 'other' if scene in ('removed','other_target') else 'source'
    damage = {'Glacial Ray':2,'Lava Spike':3,'Lightning Strike':3}.get(member,0)
    w.paid('member',budget=printed if scene == 'surplus' else None,target=target,player_target=1-actor if damage else None)
    w.emit('require_card',ref='member',property='in_hand',value=0)
    w.emit('announce_spell',ref='member')
    w.emit('assert_stack',value=1)
    if w.active() and mode == 'heroic':
        if actor == owner and target == 'source':
            w.heroic += 1
        w.emit('refresh')
        w.emit('assert_counter',ref='source',counter_kind='plus1',value=w.heroic)
    if w.active() and mode == 'piles':
        w.emit('assert_zone',player=owner,zone='library',value=len(w.zones['library'][owner]))
    if scene == 'countered':
        w.cast('reply',parent='member')
        w.finish('reply','graveyard',response=True)
        w.move('member','graveyard')
        w.receipts()
        return
    if w.active() and mode == 'piles' and actor == owner and has_subtype(record,'Sphinx'):
        w.choose_piles()
    permanent = any(item in record['type_line'].partition(' — ')[0].split() for item in ('Creature','Enchantment'))
    if damage:
        w.life[1-actor] -= damage
    if member == 'Giant Growth':
        w.growth = 3
    w.settle('member','battlefield' if permanent else 'graveyard')
    if scene == 'counter_persists':
        w.paid('bounce_aura',target='member')
        w.finish('bounce_aura','graveyard')
        w.move('member','hand')
        w.receipts()
        w.next_main()
    elif scene == 'instant_heroic':
        w.next_main()


def build_variant(name,p,scene,owner,registry,oracle):
    from recipes import step

    w = _World(name,p,owner,registry,oracle,scene)
    fixture(w)
    w.emit('turn',player=owner,value=0)
    for seat in (0,1):
        w.pool(seat,[0]*6)
    w.receipts()
    early_capture = p['mode'] == 'piles' and scene == 'replay'
    if early_capture:
        w.emit('capture')
    cut = len(w.steps)
    short = scene == 'source_short'
    budget = list(p['cost'])
    if short:
        budget[0] -= 1
    w.paid('source',allowed=not short,budget=budget)
    if short:
        w.receipts()
        return w.setup,w.steps
    if p['mode'] == 'piles':
        w.choose_piles()
    w.settle('source','battlefield')
    if not early_capture and scene == 'replay':
        w.emit('capture')
        cut = len(w.steps)
    if p['mode'] == 'regen' and scene in EXTRA['regen']:
        w.regenerate()
    elif scene.startswith('fly_'):
        w.flying()
    elif scene == 'reanimate_piles':
        w.reanimate()
    elif 'member' in w.cards:
        member_episode(w)
    if scene == 'replay':
        tail = copy.deepcopy(w.steps[cut:])
        w.steps.extend([step('record_state'),step('ai_roundtrip'),step('restore'),*tail,step('assert_recorded_state')])
    return w.setup,w.steps


def expand(name,csvid,declaration,p,selected,registry,oracle):
    row,record = registry[name],oracle[name]
    pin = profiles()['sources'].get(name)
    if not pin or pin['registry'] != row or row['csvid'] != csvid or row['handler'] != handler(record):
        raise ValueError(f'unpinned reducer source: {name}')
    cases = []
    for variant in selected:
        scene, owner = variant.rsplit('_',1)
        setup,steps = build_variant(name,p,scene,int(owner),registry,oracle)
        cases.append(dict(name=f'CastReducersWithRidersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=123,
                          setup=setup,steps=steps,_recipe=dict(family=FAMILY,version=2,variant=variant,
                          params=declaration['params'],oracle_text=declaration['oracle_text'],
                          untested_components=['multiplayer','AI pile valuation','external subtype/cost effects'])))
    return cases
