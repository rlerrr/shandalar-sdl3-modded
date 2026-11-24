"""Real subtype cost reductions and their printed permanent abilities."""

from functools import lru_cache
import json
from pathlib import Path
import re

from death_counter_trigger_recipes import exact
from multi_type_discounts_recipes import DiscountScene, card_types
from morph_turn_up_target_recipes import Scene


FAMILY = 'typed_cast_reducers'
PROFILE = Path(__file__).parent/'data/typed_cast_reducers_wave8_profiles.json'
DISCOUNT = re.compile(r'([A-Z][a-z]+) spells(?: and ([A-Z][a-z]+) spells)? you cast cost \{([12])\} less to cast\.')
ISLANDWALK = "Islandwalk (This creature can't be blocked as long as defending player controls an Island.)"
TEAM_EFFECTS = {
    'Soldier creatures you control get +1/+2.': ('Soldier', 'team_stats', (1, 2)),
    'Zombie creatures you control get +2/+1.': ('Zombie', 'team_stats', (2, 1)),
    'Goblins you control have haste.': ('Goblin', 'team_haste', (0, 0)),
}
TEAM_MODES = ('team_late_entry', 'team_source_removed', 'team_copies')
COLORED_MODELS = {
    'Edgewalker': ('Cleric', 'WB', '{1}{W}', '{1}'),
    'Ragemonger': ('Minotaur', 'BR', '{2}{R}', '{2}'),
}
COLORED_PROBES = {
    'Cleric': ("Drana's Emissary", 'Daru Spiritualist', 'Disciple of Griselbrand',
               'Order of Whiteclay', 'Scribe of the Mindful'),
    'Minotaur': ('Kragma Warcaller', 'Felhide Petrifier', 'Minotaur Sureshot',
                 'Talruum Minotaur', 'Labyrinth Minotaur'),
}
COLOR_MODES = ('first_color_only', 'second_color_only', 'unaffected_color', 'stacked_colored')
WITNESSES = {
    ('Kithkin','Soldier'): ('Kithkin Healer','Standing Troops','Preeminent Captain'),
    ('Treefolk','Shaman'): ('Yew Spirit','Bloodfire Mentor','Fendeep Summoner'),
    ('Goblin','Rogue'): ('Akki Drillmaster','Cephalid Looter','Rummaging Goblin'),
    ('Merfolk','Wizard'): ('Rootwater Hunter','Archivist','Vodalian Mage'),
    ('Dragon',): ('Dragon Whelp',),
    ('Giant',): ('Wayward Giant',),
    ('Soldier',): ('Standing Troops',),
    ('Zombie',): ('Tattered Mummy',),
    ('Goblin',): ('Goblin Tinkerer',),
    ('Cleric',): (COLORED_PROBES['Cleric'][0],),
    ('Minotaur',): (COLORED_PROBES['Minotaur'][0],),
}
KINDRED = {('Kithkin','Soldier'):"Militia's Pride",('Goblin','Rogue'):'Boggart Shenanigans',
           ('Merfolk','Wizard'):'Merrow Commerce',('Giant',):"Giant's Ire",
           ('Goblin',):'Boggart Shenanigans'}
COMMON = ('first_match','printed_budget','short','wrong_color','source_grave','source_leaves','opposing_caster',
          'source_cast','source_short','paid_replay','countered','combined','kindred_changeling','changeling_creature',
          'unrelated_creature','unrelated_noncreature','tapped_source')
HASTE = ('fresh_haste','fresh_ally')
ISLAND = ('defender_island','defender_forest','own_island','island_leaves')


@lru_cache(maxsize=1)
def pins():
    return json.loads(PROFILE.read_bytes())


def parameters(record):
    from recipes import cost_from_oracle

    if (not record or record.get('layout')!='normal' or record.get('card_faces') or record.get('selected_face')
            or card_types(record)!={'Creature'} or any(not re.fullmatch(r'\d+',record.get(k,'')) for k in ('power','toughness'))):
        raise ValueError('normal creature with fixed printed stats required')
    if record['name'] in COLORED_MODELS:
        subtype, colors, example, result = COLORED_MODELS[record['name']]
        reduction = ''.join('{' + color + '}' for color in colors)
        clause = (f'{subtype} spells you cast cost {reduction} less to cast. This effect reduces only the amount '
                  f'of colored mana you pay. (For example, if you cast a {subtype} spell with mana cost '
                  f'{example}, it costs {result} to cast.)')
        if record['oracle_text'] != clause or record['keywords']:
            raise ValueError('complete printed colored discount and example required')
        return dict(effect='subtype_discount', cost=cost_from_oracle(record), subtypes=[subtype],
                    amount=1, perk='', colors=list(colors))
    clauses = record['oracle_text'].splitlines()
    perk = ''
    team = None
    if len(clauses)==2 and clauses[0] in ('Haste',ISLANDWALK):
        perk = 'islandwalk' if clauses.pop(0)==ISLANDWALK else 'haste'
    elif len(clauses) == 2 and clauses[1] in TEAM_EFFECTS:
        team = TEAM_EFFECTS[clauses.pop()]
        _, perk, _ = team
    match = DISCOUNT.fullmatch(clauses[0]) if len(clauses)==1 else None
    keywords = {'':set(),'haste':{'Haste'},'islandwalk':{'Landwalk','Islandwalk'},
                'team_stats':set(), 'team_haste':set()}
    if not match or set(record['keywords'])!=keywords[perk]:
        raise ValueError('complete fixed subtype-union cost reduction required')
    subtypes = [s for s in match.groups()[:2] if s]
    if tuple(subtypes) not in WITNESSES:
        raise ValueError('independent mono and overlapping subtype witnesses required')
    model = dict(effect='subtype_discount', cost=cost_from_oracle(record), subtypes=subtypes,
                 amount=int(match[3]), perk=perk)
    if team is not None:
        if subtypes != [team[0]] or int(match[3]) != 1:
            raise ValueError('the printed team ability and discount must share the reviewed subtype')
        model['team_delta'] = list(team[2])
    return model


def variants(model):
    subtypes = tuple(model['subtypes'])
    modes = COMMON
    if len(subtypes)==2:
        modes += ('second_match','overlap','overlap_payment')
    if subtypes in KINDRED:
        modes += ('printed_kindred',)
    if model['perk']=='haste':
        modes += HASTE
    elif model['perk']=='islandwalk':
        modes += ISLAND
    elif model['perk'] in ('team_stats', 'team_haste'):
        modes += TEAM_MODES
        if model['perk'] == 'team_haste':
            modes += ('team_self_fresh', 'team_opponent_fresh', 'team_unrelated_fresh')
    if 'colors' in model:
        modes += COLOR_MODES
    return [f'{mode}_{actor}' for mode in modes for actor in (0,1)]


def spec(declaration,record):
    model = parameters(record)
    pin = pins()['sources'].get(record['name'],{})
    selected = declaration.get('variants')
    if (not exact(pin.get('oracle'),record) or declaration.get('family')!=FAMILY
            or type(declaration.get('version')) is not int or declaration['version']!=2
            or declaration.get('oracle_text')!=record['oracle_text'] or not exact(declaration.get('params'),model)
            or type(selected) is not list or not selected or any(type(v) is not str for v in selected)
            or len(set(selected))!=len(selected) or set(selected)-set(variants(model))):
        raise ValueError('pinned whole source and exact typed subtype variants required')
    return dict(model,family=FAMILY)


def matching_subtypes(record,subtypes):
    if not card_types(record).intersection(('Creature','Kindred')):
        return set()
    if 'Changeling' in record['keywords']:
        return set(subtypes)
    return set(record['type_line'].partition(' — ')[2].split()).intersection(subtypes)


def validate_witnesses(oracle,rows):
    from recipes import COLORS, cost_from_oracle

    profile = pins()
    if profile['version']!=1:
        raise ValueError('unreviewed fixed typed reducer schema')
    for group in ('sources','witnesses'):
        for name,pin in profile[group].items():
            row,record = rows.get(name,{}),oracle.get(name,{})
            if (not exact(pin,dict(oracle=record,registry=row)) or record.get('layout')!='normal'
                    or row.get('kind')!='card' or row.get('handler') in (None,'NULL','0','card_dummy')):
                raise ValueError(f'changed or unsupported typed reducer witness: {name}')
    for kinds,names in WITNESSES.items():
        wanted = [{t} for t in kinds] + ([set(kinds)] if len(kinds)==2 else [])
        if [matching_subtypes(oracle[n],kinds) for n in names]!=wanted:
            raise ValueError('distinguishable first, second and overlap witnesses required')
    for kinds,name in KINDRED.items():
        if (not matching_subtypes(oracle[name],kinds) or 'Kindred' not in card_types(oracle[name])
                or 'Creature' in card_types(oracle[name])):
            raise ValueError('actual printed noncreature Kindred witness required')
    for name, (subtype, colors, _, _) in COLORED_MODELS.items():
        if name not in profile['sources']:
            continue
        dual, first, second, double, other = COLORED_PROBES[subtype]
        expected = ((dual, set(colors)), (first, {colors[0]}), (second, {colors[1]}), (other, {'U'}))
        for witness, required in expected:
            price = cost_from_oracle(oracle[witness])
            actual = {color for color, index in COLORS.items() if 0 < index < len(price) and price[index]}
            if witness not in profile['witnesses'] or actual != required or not price[0]:
                raise ValueError('independent printed colored costs and generic remainders required')
        double_price = cost_from_oracle(oracle[double])
        if (double not in profile['witnesses'] or double_price[0] < 1
                or sum(double_price[1:]) != 2 or not any(double_price[COLORS[color]] == 2 for color in colors)):
            raise ValueError('exact two-symbol floor witness required')
        if any(not matching_subtypes(oracle[witness], [subtype]) for witness in COLORED_PROBES[subtype]):
            raise ValueError('every colored witness must be a qualifying spell')


def discounted_cost(record,model,copies):
    from recipes import COLORS, cost_from_oracle

    mana = cost_from_oracle(record)
    if matching_subtypes(record,model['subtypes']):
        positions = [COLORS[color] for color in model['colors']] if 'colors' in model else [0]
        for index in positions:
            mana[index] = max(0, mana[index] - model['amount'] * copies)
    return mana


class TypedScene(DiscountScene):
    # Reuse the existing cast laboratory's board, pool and ordered-zone receipts.
    def stats(self,ref):
        return Scene.stats(self,ref)

    def probe_name(self):
        kinds = tuple(self.model['subtypes'])
        roles = {'kindred_changeling':'Nameless Inversion','changeling_creature':'Amoeboid Changeling',
                 'unrelated_creature':'Unyielding Krumar','unrelated_noncreature':'Mind Stone','fresh_ally':'Rummaging Goblin'}
        if self.mode=='printed_kindred':
            return KINDRED[kinds]
        if self.mode in roles:
            return roles[self.mode]
        offset = {'second_match':1,'overlap':2,'overlap_payment':2}.get(self.mode,0)
        return WITNESSES[kinds][offset]

    def setup_cards(self):
        mode = self.mode
        zone = 'hand' if mode in ('source_cast','source_short','fresh_haste') else 'graveyard' if mode=='source_grave' else 'battlefield'
        self.add(self.name,'source',self.owner,zone,tapped=mode=='tapped_source')
        if mode=='combined':
            self.add(self.name,'copy',self.owner)
        if mode in ISLAND:
            self.add('Goblin Tinkerer','blocker',self.foe)
            land_name = 'Forest' if mode=='defender_forest' else 'Island'
            self.add(land_name,'land',self.owner if mode=='own_island' else self.foe,tapped=True)
        elif mode not in ('source_short','fresh_haste'):
            self.add(self.probe_name(),'probe',self.actor,'hand')
        if mode=='kindred_changeling':
            self.add('Wall of Tanglecord','wall',self.foe)
        if mode in ('source_leaves','island_leaves'):
            self.add('Boomerang','bounce',self.foe if mode=='source_leaves' else self.owner,'hand')
        if mode=='countered':
            self.add('Counterspell','counter',self.foe,'hand')

    def current_price(self,ref):
        card = self.entities[ref]
        copies = sum(e['zone']=='battlefield' and e['controller']==card['owner'] for key,e in self.entities.items()
                     if key in ('source','copy'))
        return discounted_cost(self.oracle[card['name']],self.model,copies)

    def start_spell(self,ref,*,budget=None,allowed=True,response=False):
        e = self.entities[ref]
        actor = e['owner']
        fee = self.current_price(ref)
        self.fund(actor,fee if budget is None else budget)
        if allowed and e['name']=='Nameless Inversion':
            self.emit('choose_target',ref='wall',player=actor)
        if allowed and e['name']=="Giant's Ire":
            self.emit('choose_player',player=actor,value=1-actor)
        self.emit('cast_response' if response else 'cast',ref=ref,value=int(allowed))
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1+int(response) if allowed else 0)
        self.prop(ref,'in_hand',not allowed)
        if allowed:
            self.pay(actor,fee)
            e['zone'] = 'stack'
        self.pools()

    def begin_source(self,allowed):
        budget = self.model['cost'].copy()
        if not allowed:
            budget[0] -= 1
        self.start_spell('source',budget=budget,allowed=allowed)
        if allowed:
            self.resolve()
            self.entities['source']['zone'] = 'battlefield'
        self.gates()

    def bounce(self,ref):
        self.cast_spell('bounce',ref)
        self.resolve()
        self.leave('bounce','graveyard')
        self.leave(ref,'hand')
        self.gates()

    def run_probe(self):
        from recipes import cost_from_oracle

        mode = self.mode
        name = self.probe_name()
        allowed = mode not in ('short','wrong_color')
        # A targeted kindred cast must reach the real choice even if the engine
        # misses its discount. Fund the printed price and test the Oracle residual.
        printed_budget = mode in ('printed_budget','overlap_payment') or name in ('Nameless Inversion',"Giant's Ire")
        budget = cost_from_oracle(self.oracle[name]) if printed_budget else self.current_price('probe')
        if mode=='short':
            budget[next(i for i,n in enumerate(budget) if n)] -= 1
        elif mode=='wrong_color':
            color = next(i for i,n in enumerate(budget) if i and n)
            budget[0] += budget[color]
            budget[color] = 0
        if mode=='paid_replay':
            self.replay = len(self.steps)
        self.start_spell('probe',budget=budget,allowed=allowed)
        if not allowed:
            self.gates()
            return
        if mode=='countered':
            self.emit('announce_spell',ref='probe')
            # The real Counterspell callback selects the announced parent itself.
            self.start_spell('counter',response=True)
            self.resolve(parent=True)
            self.leave('counter','graveyard')
            self.leave('probe','graveyard')
        else:
            self.resolve()
            if card_types(self.oracle[name]).intersection(('Instant','Sorcery')):
                self.leave('probe','graveyard')
                if name=='Nameless Inversion':
                    self.entities['wall']['bonus'] = (3,-3)
                elif name=="Giant's Ire":
                    self.life[1-self.actor] -= 4
            else:
                self.entities['probe']['zone'] = 'battlefield'
        self.gates()

    def combat(self,ref,allowed=True):
        self.emit('begin_attackers')
        self.emit('attack',ref=ref,value=int(allowed))
        self.emit('finish_attackers')
        if not allowed:
            self.gates()
            return
        self.entities[ref]['tapped'] = True
        self.emit('begin_blockers')
        blocked = 'blocker' in self.entities and self.mode!='defender_island'
        if 'blocker' in self.entities:
            self.emit('block',ref='blocker',other=ref,value=int(blocked))
        self.emit('advance_combat')
        self.prop(ref,'blocked',blocked)
        self.emit('combat_damage')
        self.resolve()
        self.emit('end_combat')
        power,toughness = self.stats(ref)
        if blocked:
            blocker_power,blocker_toughness = self.stats('blocker')
            for target,damage,tough in ((ref,blocker_power,toughness),('blocker',power,blocker_toughness)):
                if damage>=tough:
                    self.leave(target,'graveyard')
                else:
                    self.entities[target]['damage'] = damage
        else:
            self.life[self.foe] -= power
        self.gates()

    def play(self):
        self.setup_cards()
        self.refresh_zero()
        self.gates()
        mode = self.mode
        if mode in ('source_cast','source_short','fresh_haste'):
            self.begin_source(mode!='source_short')
            if mode=='source_short':
                return
            if mode=='fresh_haste':
                self.prop('source','sick',0)
                self.combat('source')
                return
        if mode=='source_leaves':
            self.bounce('source')
        if mode in ISLAND:
            if mode=='island_leaves':
                self.bounce('land')
            self.combat('source')
            return
        self.run_probe()
        if mode=='fresh_ally':
            self.prop('probe','sick',1)
            self.combat('probe',allowed=False)


class TeamScene(TypedScene):
    def setup_cards(self):
        if self.mode == 'team_self_fresh':
            self.add(self.name, 'source', self.owner, 'hand')
            return
        if self.mode == 'team_opponent_fresh':
            self.active = self.actor = self.foe
        super().setup_cards()
        if self.mode in TEAM_MODES:
            witness = WITNESSES[tuple(self.model['subtypes'])][0]
            self.add(witness, 'own_team', self.owner)
            self.add(witness, 'foe_team', self.foe)
            self.add('Unyielding Krumar', 'nonmember', self.owner)
        if self.mode == 'team_copies':
            self.add(self.name, 'copy', self.owner)
        if self.mode == 'team_source_removed':
            self.add('Boomerang', 'bounce', self.foe, 'hand')

    def probe_name(self):
        return 'Unyielding Krumar' if self.mode == 'team_unrelated_fresh' else super().probe_name()

    def stats(self, ref):
        stats = super().stats(ref)
        entity = self.entities[ref]
        record = self.oracle[entity['name']]
        if 'Creature' not in card_types(record) or not matching_subtypes(record, self.model['subtypes']):
            return stats
        copies = sum(self.entities[key]['zone'] == 'battlefield'
                     and self.entities[key]['controller'] == entity['controller']
                     for key in ('source', 'copy') if key in self.entities)
        return tuple(value + copies * delta for value, delta in zip(stats, self.model['team_delta']))

    def play(self):
        mode = self.mode
        if not mode.startswith('team_'):
            super().play()
            return
        self.setup_cards()
        self.refresh_zero()
        self.gates()
        if mode == 'team_self_fresh':
            self.begin_source(True)
            self.prop('source', 'sick', 0)
            self.combat('source')
            return
        self.run_probe()
        if mode == 'team_source_removed':
            self.bounce('source')
        if self.model['perk'] == 'team_haste':
            granted = mode in ('team_late_entry', 'team_copies')
            self.prop('probe', 'sick', int(not granted))
            self.combat('probe', allowed=granted)


class ColoredScene(TypedScene):
    def probe_name(self):
        _, first, second, double, other = COLORED_PROBES[self.model['subtypes'][0]]
        choices = {'first_color_only': first, 'second_color_only': second, 'wrong_color': double,
                   'stacked_colored': double, 'unaffected_color': other}
        if self.mode in choices:
            return choices[self.mode]
        return super().probe_name()

    def setup_cards(self):
        super().setup_cards()
        if self.mode == 'stacked_colored':
            self.add(self.name, 'copy', self.owner)


def expand(name,csvid,declaration,model,selected,rows,oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if csvid!=pins()['sources'][name]['registry']['csvid']:
        raise ValueError('unreserved typed cast reducer')
    result = []
    for variant in selected:
        mode,actor = variant.rsplit('_',1)
        scene_type = ColoredScene if 'colors' in model else TeamScene if 'team_delta' in model else TypedScene
        scene = scene_type(name,model,mode,int(actor),rows,oracle)
        scene.play()
        steps = scene.steps
        if scene.replay is not None:
            at = scene.replay
            suffix = steps[at:]
            steps = steps[:at]+[step('capture')]+suffix+[step('record_state'),step('ai_roundtrip'),step('restore')]+suffix+[step('assert_recorded_state')]
        parts = dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],oracle_text=declaration['oracle_text'],
                     covered_clauses=declaration['oracle_text'].splitlines(),untested_components=['Multiplayer'])
        result.append(dict(name=f'TypedCastReducersRecipe_{csvid}_{variant}',card=name,fixture='empty_main',seed=1830,
                           setup=scene.setup,steps=steps,_recipe=parts))
    return result
