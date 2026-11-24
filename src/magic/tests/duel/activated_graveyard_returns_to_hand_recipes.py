"""RulesEngine graveyard returns and the source cards' other printed actions."""

from collections import Counter
import copy
import json
from pathlib import Path

from cast_untap_trigger_recipes import exact
from counted_battlefield_damage_recipes import Game, types


FAMILY = 'activated_graveyard_returns_to_hand'
PROFILE = Path(__file__).parent / 'data' / f'{FAMILY}_wave8_profiles.json'
RETURN = 'Return this card from your graveyard to your hand.'
PRINTED = {
    ('Tap three untapped Clerics you control: ' + RETURN,): ('clerics', '', ('4', '4')),
    ('Jarad gets +1/+1 for each creature card in your graveyard.',
     "{1}{B}{G}, Sacrifice another creature: Each opponent loses life equal to the sacrificed creature's power.",
     'Sacrifice a Swamp and a Forest: ' + RETURN): ('lands', '', ('2', '2')),
    ('Flying', 'When this creature dies, it deals 3 damage to each creature and each player.',
     '{3}{R}{R}: ' + RETURN): ('phoenix', '{3}{R}{R}', ('3', '3')),
    ('{1}{R}, Sacrifice another creature: Tymaret deals 2 damage to target player or planeswalker.',
     '{1}{B}, Sacrifice a creature: ' + RETURN): ('tymaret', '{1}{B}', ('2', '2')),
    ('{1}{B}, Discard a card: ' + RETURN + ' Activate only during your upkeep.',
     'Cycling {1}{B} ({1}{B}, Discard this card: Draw a card.)'): ('gladiator', '{1}{B}', ('3', '1')),
    ('{2}{B}: ' + RETURN,): ('skeleton', '{2}{B}', ('1', '2')),
    ('This spell costs {X} less to cast, where X is the total mana value of noncreature artifacts you control.',
     'Sacrifice two artifacts: ' + RETURN): ('artifacts', '', ('10', '10')),
}
BASE = ('paid', 'opponent_turn', 'replay')
EXTRA = {
    'clerics': ('short', 'tapped', 'spare', 'recast'),
    'lands': ('short', 'wrong_type', 'drain', 'recast'),
    'phoenix': ('short', 'wrong_color', 'death', 'flying'),
    'tymaret': ('short', 'no_creature', 'player_damage', 'walker_damage'),
    'gladiator': ('cycle', 'cycle_short', 'cycle_opponent_turn', 'upkeep', 'upkeep_short'),
    'skeleton': ('short', 'wrong_color', 'recast'),
    'artifacts': ('short', 'artifact_creature', 'discount', 'discount_short', 'recast'),
}
CAST_MODES = {'drain', 'death', 'flying', 'player_damage', 'walker_damage', 'discount', 'discount_short'}
WITNESSES = {'Order of Whiteclay', 'Grizzly Bears', 'Giant Spider', 'Scaled Wurm',
             'Forest', 'Swamp', 'Mountain', 'Island', 'Plains',
             'Doom Blade', 'Jace Beleren', 'Darksteel Relic', 'Spellbook', 'Ornithopter',
             'Jalum Tome', 'Howling Mine', 'Cathodion'}
STATIC = {'Grizzly Bears', 'Giant Spider', 'Scaled Wurm', 'Ornithopter'}
ZONES = ('battlefield', 'hand', 'graveyard', 'library', 'exile')


def profiles():
    return json.loads(PROFILE.read_text(encoding='utf8'))


def parameters(record):
    from recipes import cost_from_oracle

    signature = tuple(record['oracle_text'].splitlines())
    shape = PRINTED.get(signature)
    if (shape is None or record.get('layout') != 'normal' or record.get('card_faces')
            or record.get('selected_face') or 'Creature' not in types(record)):
        raise ValueError('Complete normal graveyard-return creature required')
    model, fee, stats = shape
    keywords = {'phoenix': {'Flying'}, 'gladiator': {'Cycling'}}.get(model, set())
    if stats != (record.get('power'), record.get('toughness')) or set(record['keywords']) != keywords:
        raise ValueError('All printed source characteristics required')
    if model == 'artifacts':
        if record['mana_cost'] != '{11}' or 'Artifact' not in types(record):
            raise ValueError('Exact noncreature-artifact discount source required')
        printed = [11, 0, 0, 0, 0, 0]
    else:
        printed = cost_from_oracle(record)
    return dict(model=model, fee=cost_from_oracle({'mana_cost': fee}) if fee else [0]*6,
                printed=printed)


def variants(params):
    model = params['model']
    modes = EXTRA[model] if model == 'gladiator' else BASE + EXTRA[model]
    return [f'{mode}_{owner}' for owner in (0, 1) for mode in modes
            if not (model == 'gladiator' and mode.startswith('upkeep') and owner == 0)]


def spec(declaration, record):
    parsed = parameters(record)
    pin = profiles()['sources'].get(record['name'], {})
    chosen = declaration.get('variants')
    if (declaration.get('family') != FAMILY or type(declaration.get('version')) is not int
            or declaration['version'] != 2 or not exact(pin.get('oracle'), record)
            or not exact(parsed, declaration.get('params')) or declaration.get('oracle_text') != record['oracle_text']):
        raise ValueError('Exact pinned Oracle and printed costs required')
    if (not isinstance(chosen, list) or not chosen or any(type(v) is not str for v in chosen)
            or len(set(chosen)) != len(chosen) or set(chosen) - set(variants(parsed))):
        raise ValueError('Distinct reviewed graveyard-return variants required')
    return dict(parsed, family=FAMILY)


def validate_witnesses(oracle, rows):
    pinned = profiles()
    if set(pinned) != {'sources', 'witnesses'} or set(pinned['witnesses']) != WITNESSES:
        raise ValueError('Complete graveyard-return witness pins required')
    for name, entry in (pinned['sources'] | pinned['witnesses']).items():
        row = rows[name]
        if (not exact(oracle[name], entry['oracle']) or not exact(row, entry['registry'])
                or row['kind'] != 'card' or row['handler'] in ('0', 'NULL', None)
                or (row['handler'] == 'card_dummy' and name not in STATIC)):
            raise ValueError('Graveyard return identity changed: ' + name)
    if ('Cleric' not in oracle['Order of Whiteclay']['type_line']
            or sorted(types(oracle['Cathodion'])) != ['Artifact', 'Creature']
            or oracle['Scaled Wurm']['power'] != '7'):
        raise ValueError('Independent payment and rider witnesses changed')


class ReturnBoard(Game):
    def __init__(self, name, shape, mode, owner, rows, oracle):
        # Graveyard refs are original owner/index references. Seed the source
        # before that owner's other grave cards and bind its new hand ref later.
        self.shape, self.mode, self.owner, self.foe = shape, mode, owner, 1-owner
        self.rows, self.oracle = rows, oracle
        self.model = shape['model']
        self.cards, self.setup, self.steps = {}, [], []
        self.pools = [[0]*6 for _ in (0,1)]
        self.life = [20,20]
        self.replay = None
        self.cost_refs = []
        self.libraries = [[], []]
        self.loyalty = None
        self.watch = set()
        from_hand = mode in CAST_MODES or mode.startswith('cycle')
        self.add(name, 'source', owner, 'hand' if from_hand else 'graveyard')
        self.build()

    def add(self, name, ref, who, zone='battlefield', *, tapped=False):
        super().add(name, ref, who, zone, tapped=tapped)
        self.watch.add(name)

    def add_payment(self, names):
        for i,name in enumerate(names):
            ref = f'cost_{i}'
            self.add(name, ref, self.owner, tapped=self.mode == 'tapped' and i == 2)
            self.cost_refs.append(ref)

    def build(self):
        deck = ('Island','Mountain','Forest')
        for who in (0,1):
            self.add('Plains', f'spare_grave_{who}', who, 'graveyard')
            for i,name in enumerate(deck):
                ref = f'library_{who}_{i}'
                self.add(name,ref,who,'library')
                self.libraries[who].append(ref)
        if self.mode in CAST_MODES:
            self.build_rider()
            return
        if self.model == 'gladiator':
            if self.mode.startswith('upkeep'):
                self.add('Swamp','pay_black',self.owner)
                if self.mode != 'upkeep_short':
                    self.add('Swamp','pay_generic',self.owner)
                self.add('Forest','discarded',self.owner,'hand')
            return
        if self.model == 'clerics':
            count = {'short':2,'spare':4}.get(self.mode,3)
            self.add_payment(['Order of Whiteclay']*count)
            self.add('Order of Whiteclay','foreign',self.foe)
            self.add('Grizzly Bears','noncleric',self.owner)
        elif self.model == 'lands':
            costs = {'short':['Swamp'], 'wrong_type':['Swamp','Swamp']}.get(self.mode,['Forest','Swamp'])
            self.add_payment(costs)
            # A foreign Forest does not satisfy an otherwise missing Forest.
            self.add('Forest','foreign',self.foe)
        elif self.model == 'artifacts':
            names = {'short':['Darksteel Relic'], 'artifact_creature':['Darksteel Relic','Ornithopter']}.get(
                self.mode,['Darksteel Relic','Spellbook'])
            self.add_payment(names)
            self.add('Darksteel Relic','foreign',self.foe)
        elif self.model == 'tymaret':
            if self.mode != 'no_creature':
                self.add_payment(['Grizzly Bears'])
            self.add('Grizzly Bears','foreign',self.foe)
        else:
            self.add('Grizzly Bears','foreign',self.foe)

    def build_rider(self):
        if self.model == 'lands':
            self.add('Grizzly Bears','grave_creature',self.owner,'graveyard')
            self.add('Grizzly Bears','grave_foreign',self.foe,'graveyard')
            self.add('Scaled Wurm','creature_cost',self.owner)
        elif self.model == 'phoenix':
            self.add('Grizzly Bears','ground',self.foe)
            if self.mode == 'death':
                self.add('Doom Blade','removal',self.foe,'hand')
                self.add('Grizzly Bears','small_ally',self.owner)
                self.add('Scaled Wurm','large_enemy',self.foe)
        elif self.model == 'tymaret':
            self.add('Grizzly Bears','creature_cost',self.owner)
            if self.mode == 'walker_damage':
                self.add('Jace Beleren','walker',self.foe,'hand')
        elif self.model == 'artifacts':
            for name,ref in (('Jalum Tome','artifact_three'),('Howling Mine','artifact_two'),('Cathodion','artifact_creature')):
                self.add(name,ref,self.owner)
            self.add('Jalum Tome','foreign',self.foe)

    def stat(self, ref, prop, value):
        self.emit('assert_card',ref=ref,property=prop,value=value)

    def zone_count(self, name, who, zone, value):
        self.emit('assert_zone_multiplicity',player=who,zone=zone,index=self.rows[name]['csvid'],value=value)

    def draw(self, who):
        ref = self.libraries[who].pop(0)
        self.cards[ref]['zone'] = 'hand'

    def receipts(self, *, stack=0):
        self.emit('refresh')
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=stack)
        self.observe_mana()
        quantities = Counter((item['player'],item['zone'],item['name']) for item in self.cards.values())
        for who in (0,1):
            self.emit('assert_life',player=who,value=self.life[who])
            for zone in ZONES:
                self.emit('assert_zone',player=who,zone=zone,
                          value=sum(n for (seat,area,_),n in quantities.items() if (seat,area) == (who,zone)))
                for name in sorted(self.watch):
                    self.zone_count(name,who,zone,quantities[who,zone,name])
            for i,ref in enumerate(self.libraries[who]):
                self.emit('assert_zone_card',player=who,zone='library',index=i,value=self.rows[self.cards[ref]['name']]['csvid'])
        for ref in ('foreign', *self.cost_refs):
            if ref in self.cards and self.cards[ref]['zone'] == 'battlefield':
                self.stat(ref,'tapped',int(self.cards[ref]['tapped']))
        if self.loyalty is not None:
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=self.loyalty)

    def return_choices(self):
        self.emit('choose_dialog', player=self.owner, value=1)
        if self.model in ('clerics','lands') or self.owner == 0:
            paid = self.cost_refs[:3] if self.model == 'clerics' else self.cost_refs
            for ref in paid:
                self.emit('choose_target',player=self.owner,ref=ref)

    def return_costs(self):
        if self.model == 'clerics':
            for ref in self.cost_refs[:3]:
                self.cards[ref]['tapped'] = True
                self.stat(ref,'tapped',1)
            if len(self.cost_refs) == 4:
                self.stat(self.cost_refs[3],'tapped',0)
            self.stat('noncleric','tapped',0)
        else:
            for ref in self.cost_refs:
                self.cards[ref]['zone'] = 'graveyard'
            for name in sorted({self.cards[ref]['name'] for ref in self.cost_refs}):
                self.zone_count(name,self.owner,'graveyard',sum(
                    item['name']==name and item['player']==self.owner and item['zone']=='graveyard'
                    for item in self.cards.values()))
        self.observe_mana()

    def return_ability(self):
        source = self.cards['source']
        self.emit('prepare_graveyard_abilities')
        pool = list(self.shape['fee'])
        if self.mode == 'short' and any(pool):
            pool[0] -= 1
        if self.mode == 'wrong_color':
            chosen = next(i for i in range(1,6) if pool[i])
            pool[5],pool[chosen] = pool[chosen],0
        self.fund(self.owner,pool)
        denied = self.mode in ('short','tapped','wrong_type','wrong_color','no_creature')
        if denied:
            self.emit('require_can_activate_graveyard',ref='source',value=0)
            self.emit('activate_graveyard',ref='source',value=0)
            self.receipts()
            return
        if self.mode == 'replay':
            self.replay = len(self.steps)
        # A real rejected printed cost is an ordinary assertion here. It
        # terminates before a genuine but unoffered menu can be queued.
        self.emit('require_can_activate_graveyard',ref='source',value=1)
        self.return_choices()
        self.emit('activate_graveyard',ref='source',value=1)
        self.emit('assert_choices_consumed')
        self.emit('assert_stack',value=1)
        self.pools[self.owner] = [0]*6
        self.return_costs()
        self.zone_count(source['name'],self.owner,'graveyard',1)
        self.zone_count(source['name'],self.owner,'hand',0)
        self.emit('resolve')
        source['zone'] = 'hand'
        self.receipts()
        if self.mode == 'recast':
            self.emit('bind_hand',ref='source',player=self.owner,index=-1,value=self.rows[source['name']]['csvid'])
            self.cast_printed()
            self.receipts()

    def cast_printed(self, *, expected=True, price=None):
        pool = list(self.shape['printed'] if price is None else price)
        if not expected:
            pool[0] -= 1
        self.fund(self.owner,pool)
        self.emit('cast',ref='source',value=int(expected))
        self.emit('assert_stack',value=int(expected))
        if expected:
            self.pools[self.owner] = [0]*6
            self.emit('resolve')
            self.cards['source']['zone'] = 'battlefield'
            self.emit('refresh')
            self.stat('source','in_play',1)

    def combat_flying(self):
        self.emit('advance_turn')
        self.emit('untap_step',player=self.foe)
        self.emit('advance_turn')
        self.emit('untap_step',player=self.owner)
        self.emit('turn',player=self.owner)
        self.emit('begin_attackers')
        self.emit('attack',ref='source',value=1)
        self.emit('finish_attackers')
        self.emit('begin_blockers')
        self.emit('block',ref='ground',other='source',value=0)
        self.emit('advance_combat')
        self.emit('combat_damage')
        self.emit('resolve')
        self.emit('end_combat')
        self.cards['source']['tapped'] = True
        self.life[self.foe] -= 3

    def death_damage(self):
        self.cast('removal',target='source')
        self.emit('resolve')
        for ref in ('removal','source','ground','small_ally'):
            self.cards[ref]['zone'] = 'graveyard'
        self.life = [17,17]
        self.stat('large_enemy','damage',3)

    def sacrifice_power_or_damage(self):
        from recipes import cost_from_oracle

        jarad = self.model == 'lands'
        if self.mode == 'walker_damage':
            self.emit('turn',player=self.foe)
            self.cast('walker')
            self.emit('resolve')
            self.cards['walker']['zone'] = 'battlefield'
            self.emit('turn',player=self.owner)
            self.loyalty = 3
            self.emit('assert_counter',ref='walker',counter_kind='loyalty',value=3)
        self.cast_printed()
        if jarad:
            self.stat('source','power',3)
            self.stat('source','toughness',3)
        oracle = self.oracle[self.cards['source']['name']]
        clause = next(line for line in oracle['oracle_text'].splitlines() if ', Sacrifice another creature:' in line)
        fee = cost_from_oracle({'mana_cost':clause.split(', ',1)[0]})
        self.fund(self.owner,fee)
        self.emit('require_can_activate',ref='source',value=1)
        if self.owner == 0:
            self.emit('choose_target',player=self.owner,ref='creature_cost')
        if self.mode == 'player_damage':
            self.emit('choose_player',player=self.owner,value=self.foe)
        elif self.mode == 'walker_damage':
            self.emit('choose_target',player=self.owner,ref='walker')
        self.emit('activate',ref='source',value=1)
        self.pools[self.owner] = [0]*6
        self.cards['creature_cost']['zone'] = 'graveyard'
        self.receipts(stack=1)
        self.emit('resolve')
        if jarad:
            self.life[self.foe] -= int(self.oracle[self.cards['creature_cost']['name']]['power'])
            self.stat('source','power',4)
            self.stat('source','toughness',4)
        elif self.loyalty is not None:
            self.loyalty -= 2
        else:
            self.life[self.foe] -= 2
        self.receipts()

    def run_gladiator(self):
        if self.mode.startswith('upkeep'):
            self.emit('prepare_graveyard_abilities')
            self.emit('advance_turn')
            self.emit('untap_step',player=self.owner)
            if self.mode == 'upkeep':
                self.emit('choose_dialog',player=self.owner,value=0)
                self.cards['source']['zone'] = 'hand'
                self.cards['discarded']['zone'] = 'graveyard'
            self.emit('upkeep_step',player=self.owner)
            # An absent dispatch is independently observable in the mana-source
            # and zone receipts. Retire the planned real optional choice safely.
            self.emit('assert_unused_choices',value=0)
            for ref in ('pay_black','pay_generic'):
                if ref in self.cards:
                    self.stat(ref,'tapped',int(self.mode == 'upkeep'))
            self.receipts()
            return
        amount = list(self.shape['fee'])
        denied = self.mode == 'cycle_short'
        if denied:
            amount[0] -= 1
        self.fund(self.owner,amount)
        self.emit('require_can_activate_hand',ref='source',value=int(not denied))
        if not denied:
            self.emit('choose_dialog',player=self.owner,value=1)
        self.emit('activate_hand',ref='source',value=int(not denied))
        if not denied:
            self.pools[self.owner] = [0]*6
            self.cards['source']['zone'] = 'graveyard'
            self.receipts(stack=1)
            self.emit('resolve')
            self.draw(self.owner)
        self.receipts()

    def run(self):
        foe_turn = self.mode in ('opponent_turn','cycle_opponent_turn') or self.mode.startswith('upkeep')
        self.emit('turn',player=self.foe if foe_turn else self.owner)
        self.emit('present_ai_dialogs',value=1)
        self.fund(self.owner,[0]*6)
        self.fund(self.foe,[0]*6)
        if self.model == 'gladiator':
            self.run_gladiator()
        elif self.mode in ('drain','player_damage','walker_damage'):
            self.sacrifice_power_or_damage()
        elif self.mode in ('discount','discount_short'):
            total = sum(int(self.oracle[item['name']]['cmc']) for item in self.cards.values()
                        if item['player']==self.owner and item['zone']=='battlefield'
                        and 'Artifact' in types(self.oracle[item['name']])
                        and 'Creature' not in types(self.oracle[item['name']]))
            self.cast_printed(expected=self.mode=='discount',price=[self.shape['printed'][0]-total,0,0,0,0,0])
            self.receipts()
        elif self.mode in ('death','flying'):
            self.cast_printed()
            self.death_damage() if self.mode=='death' else self.combat_flying()
            self.receipts()
        else:
            self.return_ability()


def expand(name, csvid, declaration, params, selected, rows, oracle):
    from recipes import step

    validate_witnesses(oracle,rows)
    if (rows[name]['csvid'] != csvid or not selected or len(set(selected)) != len(selected)
            or set(selected) - set(variants(params))):
        raise ValueError('Only reviewed registered graveyard-return variants may expand')
    cases = []
    for variant in selected:
        mode,owner = variant.rsplit('_',1)
        board = ReturnBoard(name,params,mode,int(owner),rows,oracle)
        board.run()
        steps = board.steps
        if board.replay is not None:
            prefix,suffix = steps[:board.replay],steps[board.replay:]
            steps = prefix+[step('capture')]+suffix+[step('record_state'),step('restore'),
                    *copy.deepcopy(suffix),step('assert_recorded_state')]
        gaps = ['Effects that respond to the graveyard activation and ownership replacement effects.']
        if params['model']=='gladiator':
            gaps = ['Human upkeep return requires selecting a category-four graveyard card through the RulesEngine test adapter. The current adapter cannot address that source. The production AI upkeep route and real cycling are covered.']
        cases.append(dict(name=f'ActivatedGraveyardReturnToHandRecipe_{csvid}_{variant}',card=name,
                          fixture='empty_main',seed=123,setup=board.setup,steps=steps,
                          _recipe=dict(family=FAMILY,version=2,variant=variant,params=declaration['params'],
                                       oracle_text=declaration['oracle_text'],untested_components=gaps)))
    return cases
